// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.httpflags.BaseFeature;
import org.chromium.net.httpflags.Flags;
import org.chromium.net.httpflags.HttpFlagsLoader;
import org.chromium.net.httpflags.ResolvedFlags;

/**
 * CronetLibraryLoader loads and initializes native library on init thread.
 */
@JNINamespace("cronet")
@VisibleForTesting
public class CronetLibraryLoader {
    // Synchronize initialization.
    private static final Object sLoadLock = new Object();
    private static final String LIBRARY_NAME = "cronet." + ImplVersion.getCronetVersion();
    @VisibleForTesting
    public static final String TAG = CronetLibraryLoader.class.getSimpleName();
    // Thread used for initialization work and processing callbacks for
    // long-lived global singletons. This thread lives forever as things like
    // the global singleton NetworkChangeNotifier live on it and are never killed.
    private static final HandlerThread sInitThread = new HandlerThread("CronetInit");
    // Has library loading commenced?  Setting guarded by sLoadLock.
    private static volatile boolean sLibraryLoaded;
    // Has ensureInitThreadInitialized() completed?
    private static volatile boolean sInitThreadInitDone;
    // Block calling native methods until this ConditionVariable opens to indicate loadLibrary()
    // is completed and native methods have been registered.
    private static final ConditionVariable sWaitForLibLoad = new ConditionVariable();

    private static final ConditionVariable sHttpFlagsLoaded = new ConditionVariable();
    private static ResolvedFlags sHttpFlags;

    @VisibleForTesting
    public static final String LOG_FLAG_NAME = "Cronet_log_me";

    /**
     * Ensure that native library is loaded and initialized. Can be called from
     * any thread, the load and initialization is performed on init thread.
     */
    public static void ensureInitialized(
            Context applicationContext, final CronetEngineBuilderImpl builder) {
        synchronized (sLoadLock) {
            if (!sInitThreadInitDone) {
                ContextUtils.initApplicationContext(applicationContext);
                if (!sInitThread.isAlive()) {
                    sInitThread.start();
                }
                postToInitThread(new Runnable() {
                    @Override
                    public void run() {
                        ensureInitializedOnInitThread();
                    }
                });
            }
            if (!sLibraryLoaded) {
                if (builder.libraryLoader() != null) {
                    builder.libraryLoader().loadLibrary(LIBRARY_NAME);
                } else {
                    System.loadLibrary(LIBRARY_NAME);
                }
                String implVersion = ImplVersion.getCronetVersion();
                if (!implVersion.equals(CronetLibraryLoaderJni.get().getCronetVersion())) {
                    throw new RuntimeException(String.format("Expected Cronet version number %s, "
                                    + "actual version number %s.",
                            implVersion, CronetLibraryLoaderJni.get().getCronetVersion()));
                }
                Log.i(TAG, "Cronet version: %s, arch: %s", implVersion,
                        System.getProperty("os.arch"));
                setNativeLoggingLevel();
                sLibraryLoaded = true;
                sWaitForLibLoad.open();
            }
        }
    }

    private static void setNativeLoggingLevel() {
        // The constants used here should be kept in sync with logging::LogMessage::~LogMessage().
        final String nativeLogTag = "chromium";
        int loggingLevel;
        // TODO: this way of enabling VLOG is a hack - it doesn't make a ton of sense because
        // logging::LogMessage() will still log VLOG() at the Android INFO log level, not DEBUG or
        // VERBOSE; also this doesn't make it possible to use advanced filters like --vmodule. See
        // https://crbug.com/1488393 for a proposed alternative.
        if (Log.isLoggable(nativeLogTag, Log.VERBOSE)) {
            loggingLevel = -2; // VLOG(2)
        } else if (Log.isLoggable(nativeLogTag, Log.DEBUG)) {
            loggingLevel = -1; // VLOG(1)
        } else {
            // Use the default log level, which logs everything except VLOG(). Skip the
            // setMinLogLevel() call to avoid paying for an unnecessary JNI transition.
            return;
        }
        CronetLibraryLoaderJni.get().setMinLogLevel(loggingLevel);
    }

    /**
     * Returns {@code true} if running on the initialization thread.
     */
    private static boolean onInitThread() {
        return sInitThread.getLooper() == Looper.myLooper();
    }

    /**
     * Ensure that the init thread initialization has completed. Can only be called from
     * the init thread. Ensures that HTTP flags are loaded, the NetworkChangeNotifier is initialzied
     * and the init thread native MessageLoop is initialized.
     */
    static void ensureInitializedOnInitThread() {
        assert onInitThread();
        if (sInitThreadInitDone) {
            return;
        }

        // Load HTTP flags. This is a potentially expensive call, so we do this in parallel with
        // library loading in the hope of minimizing impact on Cronet initialization latency.
        assert sHttpFlags == null;
        Context applicationContext = ContextUtils.getApplicationContext();
        Flags flags = HttpFlagsLoader.load(applicationContext);
        sHttpFlags = ResolvedFlags.resolve(flags != null ? flags : Flags.newBuilder().build(),
                applicationContext.getPackageName(), ImplVersion.getCronetVersion());
        sHttpFlagsLoaded.open();
        ResolvedFlags.Value logMe = sHttpFlags.flags().get(LOG_FLAG_NAME);
        if (logMe != null) {
            Log.i(TAG, "HTTP flags log line: %s", logMe.getStringValue());
        }

        NetworkChangeNotifier.init();
        // Registers to always receive network notifications. Note
        // that this call is fine for Cronet because Cronet
        // embedders do not have API access to create network change
        // observers. Existing observers in the net stack do not
        // perform expensive work.
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        // Wait for loadLibrary() to complete so JNI is registered.
        sWaitForLibLoad.block();
        assert sLibraryLoaded;

        // TODO: override native base::Feature flags based on `resolvedFlags`. Note that this might
        // be tricky because we can only set up base::Feature overrides after the .so is loaded, but
        // we have to do it before any native code runs and tries to use any base::Feature flag.

        // registerToReceiveNotificationsAlways() is called before the native
        // NetworkChangeNotifierAndroid is created, so as to avoid receiving
        // the undesired initial network change observer notification, which
        // will cause active requests to fail with ERR_NETWORK_CHANGED.
        CronetLibraryLoaderJni.get().cronetInitOnInitThread();
        sInitThreadInitDone = true;
    }

    /**
     * Run {@code r} on the initialization thread.
     */
    public static void postToInitThread(Runnable r) {
        if (onInitThread()) {
            r.run();
        } else {
            new Handler(sInitThread.getLooper()).post(r);
        }
    }

    /**
     * Called by native Cronet library early initialization code to obtain the values of
     * native base::Feature overrides that will be applied for the entire lifetime of the Cronet
     * native library.
     *
     * <p>Note that this call sits in the critical path of native library initialization, as
     * practically no Chromium native code can run until base::Feature values have settled.
     *
     * @return The base::Feature overrides as a binary serialized {@link
     * org.chromium.net.httpflags.BaseFeatureOverrides} proto.
     */
    @CalledByNative
    private static byte[] getBaseFeatureOverrides() {
        sHttpFlagsLoaded.block();
        return BaseFeature.getOverrides(sHttpFlags).toByteArray();
    }

    /**
     * Called from native library to get default user agent constructed
     * using application context. May be called on any thread.
     *
     * Expects that ContextUtils.initApplicationContext() was called already
     * either by some testing framework or an embedder constructing a Java
     * CronetEngine via CronetEngine.Builder.build().
     */
    @CalledByNative
    private static String getDefaultUserAgent() {
        return UserAgent.from(ContextUtils.getApplicationContext());
    }

    /**
     * Called from native library to ensure that library is initialized.
     * May be called on any thread, but initialization is performed on
     * this.sInitThread.
     *
     * Expects that ContextUtils.initApplicationContext() was called already
     * either by some testing framework or an embedder constructing a Java
     * CronetEngine via CronetEngine.Builder.build().
     *
     * TODO(mef): In the long term this should be changed to some API with
     * lower overhead like CronetEngine.Builder.loadNativeCronet().
     */
    @CalledByNative
    private static void ensureInitializedFromNative() {
        // Called by native, so native library is already loaded.
        // It is possible that loaded native library is not regular
        // "libcronet.xyz.so" but test library that statically links
        // native code like "libcronet_unittests.so".
        synchronized (sLoadLock) {
            sLibraryLoaded = true;
            sWaitForLibLoad.open();
        }

        // The application context must already be initialized
        // using ContextUtils.initApplicationContext().
        Context applicationContext = ContextUtils.getApplicationContext();
        assert applicationContext != null;
        ensureInitialized(applicationContext, null);
    }

    @CalledByNative
    private static void setNetworkThreadPriorityOnNetworkThread(int priority) {
        Process.setThreadPriority(priority);
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in cronet_library_loader.cc.
        void cronetInitOnInitThread();

        String getCronetVersion();

        void setMinLogLevel(int loggingLevel);
    }
}
