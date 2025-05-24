// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.net.NetLogCaptureMode;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.RegistrationPolicyAlwaysRegister;
import org.chromium.net.httpflags.BaseFeature;

import javax.annotation.concurrent.GuardedBy;

/** CronetLibraryLoader loads and initializes native library on init thread. */
@JNINamespace("cronet")
@VisibleForTesting
public class CronetLibraryLoader {
    // Synchronize initialization.
    private static final Object sLoadLock = new Object();

    @GuardedBy("sLoadLock")
    private static boolean sInitialized;

    private static final String LIBRARY_NAME = "cronet." + ImplVersion.getCronetVersion();
    private static final String TESTING_LIBRARY_NAME = LIBRARY_NAME + "_for_testing";
    private static boolean sSwitchToTestLibrary;
    @VisibleForTesting public static final String TAG = CronetLibraryLoader.class.getSimpleName();
    // Thread used for initialization work and processing callbacks for
    // long-lived global singletons. This thread lives forever as things like
    // the global singleton NetworkChangeNotifier live on it and are never killed.
    private static final HandlerThread sInitThread = new HandlerThread("CronetInit");
    // Block calling native methods until this ConditionVariable opens to indicate loadLibrary()
    // is completed and native methods have been registered.
    private static final ConditionVariable sWaitForLibLoad = new ConditionVariable();

    private static final ConditionVariable sHttpFlagsLoaded = new ConditionVariable();

    @VisibleForTesting
    public static final String TRACE_NET_LOG_SYSTEM_PROPERTY_KEY = "debug.cronet.trace_netlog";

    @VisibleForTesting
    public static final String UPDATE_NETWORK_STATE_ONCE_ON_STARTUP_FLAG_NAME =
            "Cronet_UpdateNetworkStateOnlyOnceOnStartup";

    @VisibleForTesting
    public static final String INITIALIZE_BUILD_INFO_ON_STARTUP =
            "Cronet_InitializeBuildInfoOnStartup";

    /**
     * Ensure that native library is loaded and initialized. Can be called from any thread, the load
     * and initialization is performed on init thread.
     *
     * @return True if the library was initialized as part of this call, false if it was already
     *     initialized.
     */
    public static boolean ensureInitialized(
            Context applicationContext, final CronetEngineBuilderImpl builder) {
        return ensureInitialized(applicationContext, builder, /* libAlreadyLoaded= */ false);
    }

    /**
     * This method will be called by the Zygote pre-fork to preload the native code. Which means
     * that this will be dead code in Chromium but it will be used in AOSP.
     */
    public static void preload() {
        loadLibrary();
    }

    @VisibleForTesting
    public static void loadLibrary() {
        if (sSwitchToTestLibrary) {
            System.loadLibrary(TESTING_LIBRARY_NAME);
        } else {
            System.loadLibrary(LIBRARY_NAME);
        }
    }

    @VisibleForTesting
    public static void switchToTestLibrary() {
        sSwitchToTestLibrary = true;
    }

    public static boolean ensureInitialized(
            Context applicationContext,
            final CronetEngineBuilderImpl builder,
            boolean libAlreadyLoaded) {
        try (var traceEvent = ScopedSysTraceEvent.scoped("CronetLibraryLoader#ensureInitialized")) {
            synchronized (sLoadLock) {
                if (sInitialized) return false;

                // Cronet doesn't currently provide any way of using a custom command line
                // (see https://crbug.com/1488393). For now, initialize an empty command line
                // so that code attempting to use the command line doesn't crash.
                CommandLine.init(new String[] {"cronet"});

                ContextUtils.initApplicationContext(applicationContext);

                // The init thread may already be running if a previous initialization attempt
                // failed. In this case there is no need to spin it up again.
                //
                // Note: if we never succeed in loading the library, the init thread will end up
                // blocking on `sWaitForLibLoad` forever. Obviously this is suboptimal, but given
                // this is not supposed to fail, it's arguably benign.
                if (!sInitThread.isAlive()) {
                    try (var startInitThreadTraceEvent =
                            ScopedSysTraceEvent.scoped(
                                    "CronetLibraryLoader#ensureInitialized starting init thread")) {
                        sInitThread.start();
                        postToInitThread(
                                () -> {
                                    initializeOnInitThread();
                                });
                    }
                }
                if (!libAlreadyLoaded) {
                    try (var loadLibTraceEvent =
                            ScopedSysTraceEvent.scoped(
                                    "CronetLibraryLoader#ensureInitialized loading native"
                                            + " library")) {
                        if (builder.libraryLoader() != null) {
                            builder.libraryLoader().loadLibrary(LIBRARY_NAME);
                        } else {
                            loadLibrary();
                        }
                    }
                }
                try (var nativeInitTraceEvent =
                        ScopedSysTraceEvent.scoped(
                                "CronetLibraryLoader#ensureInitialized calling nativeInit")) {
                    CommandLine.getInstance().switchToNativeImpl();
                    CronetLibraryLoaderJni.get()
                            .nativeInit(CronetManifest.shouldUsePerfetto(applicationContext));
                }
                var initializeBuildInfoOnStartup =
                        HttpFlagsForImpl.getHttpFlags(ContextUtils.getApplicationContext())
                                .flags()
                                .get(INITIALIZE_BUILD_INFO_ON_STARTUP);

                // The flag is considered active if it is absent unlike the usual case
                // where the flag is considered active only if it's "true". This is needed
                // to ensure we don't change the behaviour.
                if (initializeBuildInfoOnStartup == null
                        || initializeBuildInfoOnStartup.getBoolValue()) {
                    // This is added here to maintain the previous behaviour of Cronet where
                    // it would initialize BuildInfo when it calls `getCronetVersion` in the
                    // proceeding line. We want to A/B on the impact of removing this.
                    BuildInfo.getInstance();
                }
                String implVersion = ImplVersion.getCronetVersion();
                if (!implVersion.equals(CronetLibraryLoaderJni.get().getCronetVersion())) {
                    throw new RuntimeException(
                            String.format(
                                    "Expected Cronet version number %s, "
                                            + "actual version number %s.",
                                    implVersion, CronetLibraryLoaderJni.get().getCronetVersion()));
                }
                Log.i(
                        TAG,
                        "Cronet version: %s, arch: %s",
                        implVersion,
                        System.getProperty("os.arch"));
                setNativeLoggingLevel();
                TraceEvent.onNativeTracingReady();
                sWaitForLibLoad.open();
                sInitialized = true;
                return true;
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

    /** Returns {@code true} if running on the initialization thread. */
    private static boolean onInitThread() {
        return sInitThread.getLooper() == Looper.myLooper();
    }

    private static @NetLogCaptureMode int getTraceNetLogCaptureMode() {
        @NetLogCaptureMode int traceNetLogCaptureMode = NetLogCaptureMode.HEAVILY_REDACTED;
        var requestedTraceNetLogCaptureMode =
                AndroidOsSystemProperties.get(
                        TRACE_NET_LOG_SYSTEM_PROPERTY_KEY, "heavily_redacted");
        if (requestedTraceNetLogCaptureMode.equals("heavily_redacted")) {
            traceNetLogCaptureMode = NetLogCaptureMode.HEAVILY_REDACTED;
        } else if (requestedTraceNetLogCaptureMode.equals("on")) {
            // Note DEFAULT is mapped to "on", not "default", to avoid confusion with regard to
            // the default value of the system property.
            traceNetLogCaptureMode = NetLogCaptureMode.DEFAULT;
        } else if (requestedTraceNetLogCaptureMode.equals("include_sensitive")) {
            traceNetLogCaptureMode = NetLogCaptureMode.INCLUDE_SENSITIVE;
        } else if (requestedTraceNetLogCaptureMode.equals("everything")) {
            traceNetLogCaptureMode = NetLogCaptureMode.EVERYTHING;
        } else {
            Log.w(
                    TAG,
                    "Unknown value for %s system property, ignoring: %s",
                    TRACE_NET_LOG_SYSTEM_PROPERTY_KEY,
                    requestedTraceNetLogCaptureMode);
        }

        if (traceNetLogCaptureMode > NetLogCaptureMode.HEAVILY_REDACTED) {
            final var buildType = AndroidOsBuild.get().getType();
            if (!buildType.equals("userdebug")
                    && !buildType.equals("eng")
                    && (ContextUtils.getApplicationContext().getApplicationInfo().flags
                                    & ApplicationInfo.FLAG_DEBUGGABLE)
                            == 0) {
                Log.w(
                        TAG,
                        "Ignoring requested Cronet trace netlog capture mode (%s=%s) because"
                                + " neither the device nor app are debuggable",
                        TRACE_NET_LOG_SYSTEM_PROPERTY_KEY,
                        requestedTraceNetLogCaptureMode);
                traceNetLogCaptureMode = NetLogCaptureMode.HEAVILY_REDACTED;
            }
        }

        return traceNetLogCaptureMode;
    }

    /**
     * Runs Cronet initialization tasks on the init thread. Ensures that HTTP flags are loaded, the
     * NetworkChangeNotifier is initialzied and the init thread native MessageLoop is initialized.
     */
    static void initializeOnInitThread() {
        try (var traceEvent =
                ScopedSysTraceEvent.scoped("CronetLibraryLoader#initializeOnInitThread")) {
            assert onInitThread();
            // TODO: this may be more trouble than it's worth now that we load the flags from the
            // API beforehand anyway. We could simplify the code to remove this optimization and it
            // likely wouldn't make any difference.
            // Load and initialize httpflags in parallel with Cronet loading
            // as an attempt to alleviate the critical path blocking.
            HttpFlagsForImpl.getHttpFlags(ContextUtils.getApplicationContext());
            sHttpFlagsLoaded.open();
            NetworkChangeNotifier.init();
            // Registers to always receive network notifications. Note
            // that this call is fine for Cronet because Cronet
            // embedders do not have API access to create network change
            // observers. Existing observers in the net stack do not
            // perform expensive work.
            //
            // During the setup of connectivity state autodetection, the network state is updated
            // multiple times:
            // 1. Within Java NetworkChangeNotifierAutoDetect's constructor
            // 2. Within Java NetworkChangeNotifier#setAutoDetectConnectivityStateInternal, after
            // creating a NetworkChangeNotifierAutoDetect (effectively, just after 1)
            // 3. Within C++ NetworkChangeNotifierDelegateAndroid's constructor
            //
            // 2 should never be needed, as 1 always runs before and takes care of updating the
            // network state. Having said that, it will be kept to keep track of the performance
            // improvement from this change. Once the experiment terminates, we will delete it, this
            // should always be safe for Chrome, Cronet and Webview.
            //
            // As per 3, Cronet always initializes NetworkChangeNotifier first from Java (going
            // through 1 and 2), then from C++ (going through 3).
            // Since we would like to query the network state only once, this experiment
            // disables 2 and 3.
            var updateNetworkStateOnceFlagValue =
                    HttpFlagsForImpl.getHttpFlags(ContextUtils.getApplicationContext())
                            .flags()
                            .get(UPDATE_NETWORK_STATE_ONCE_ON_STARTUP_FLAG_NAME);
            var updateNetworkStateOnce =
                    updateNetworkStateOnceFlagValue != null
                            && updateNetworkStateOnceFlagValue.getBoolValue();
            NetworkChangeNotifier.setAutoDetectConnectivityState(
                    new RegistrationPolicyAlwaysRegister(),
                    /* forceUpdateNetworkState= */ !updateNetworkStateOnce);

            final var traceNetLogCaptureMode = getTraceNetLogCaptureMode();

            try (var libLoadTraceEvent =
                    ScopedSysTraceEvent.scoped(
                            "CronetLibraryLoader#initializeOnInitThread waiting on library load")) {
                // Wait for loadLibrary() to complete so JNI is registered.
                sWaitForLibLoad.block();
            }

            try (var nativeInitTraceEvent =
                    ScopedSysTraceEvent.scoped(
                            "CronetLibraryLoader#ensureInitialized calling"
                                    + " cronetInitOnInitThread")) {
                // registerToReceiveNotificationsAlways() is called before the native
                // NetworkChangeNotifierAndroid is created, so as to avoid receiving
                // the undesired initial network change observer notification, which
                // will cause active requests to fail with ERR_NETWORK_CHANGED.
                CronetLibraryLoaderJni.get()
                        .cronetInitOnInitThread(!updateNetworkStateOnce, traceNetLogCaptureMode);
            }
        }
    }

    public static @NetLogCaptureMode int getTraceNetLogCaptureModeForTesting() {
        return CronetLibraryLoaderJni.get().getTraceNetLogCaptureModeForTesting(); // IN-TEST
    }

    /** Run {@code r} on the initialization thread. */
    public static void postToInitThread(Runnable r) {
        if (onInitThread()) {
            r.run();
        } else {
            new Handler(sInitThread.getLooper()).post(r);
        }
    }

    /**
     * Called by native Cronet library early initialization code to obtain the values of native
     * base::Feature overrides that will be applied for the entire lifetime of the Cronet native
     * library.
     *
     * <p>Note that this call sits in the critical path of native library initialization, as
     * practically no Chromium native code can run until base::Feature values have settled.
     *
     * @return The base::Feature overrides as a binary serialized {@link
     *     org.chromium.net.httpflags.BaseFeatureOverrides} proto.
     */
    @CalledByNative
    private static byte[] getBaseFeatureOverrides() {
        return BaseFeature.getOverrides(
                        HttpFlagsForImpl.getHttpFlags(ContextUtils.getApplicationContext()))
                .toByteArray();
    }

    /**
     * Called from native library to get default user agent constructed using application context.
     * May be called on any thread.
     *
     * <p>Expects that ContextUtils.initApplicationContext() was called already either by some
     * testing framework or an embedder constructing a Java CronetEngine via
     * CronetEngine.Builder.build().
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
        // The application context must already be initialized
        // using ContextUtils.initApplicationContext().
        Context applicationContext = ContextUtils.getApplicationContext();
        assert applicationContext != null;
        ensureInitialized(applicationContext, null, /* libAlreadyLoaded= */ true);
    }

    @CalledByNative
    private static void setNetworkThreadPriorityOnNetworkThread(int priority) {
        Log.d(TAG, "Setting network thread priority to " + priority);
        Process.setThreadPriority(priority);
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in cronet_library_loader.cc.
        void nativeInit(boolean initializePerfetto);

        void cronetInitOnInitThread(
                boolean updateNetworkStateFromNative,
                @NetLogCaptureMode @JniType("net::NetLogCaptureMode") int traceNetLogCaptureMode);

        @NetLogCaptureMode
        @JniType("net::NetLogCaptureMode")
        int getTraceNetLogCaptureModeForTesting(); // IN-TEST

        String getCronetVersion();

        void setMinLogLevel(int loggingLevel);
    }
}
