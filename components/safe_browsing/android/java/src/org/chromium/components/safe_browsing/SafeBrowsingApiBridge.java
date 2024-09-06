// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.GuardedBy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler.LookupResult;

/**
 * Helper for calling GMSCore APIs from native code to perform Safe Browsing checks.
 *
 * <p>{@link #setSafetyNetApiHandler(SafetyNetApiHandler)} and {@link
 * #setSafeBrowsingApiHandler(SafeBrowsingApiHandler)} must be invoked first. After that {@link
 * #startUriLookupBySafeBrowsingApi(long, String, int[], int)}, {@link #startAllowlistLookup(String,
 * int)} and {@link #isVerifyAppsEnabled(long)} can be used to check the URLs.
 *
 * <p>Optionally calling {@link #ensureSafetyNetApiInitialized()} allows initializing the
 * SafetyNetApiHandler eagerly. Calling {@link #initSafeBrowsingApi()} allows initializing the
 * SafeBrowsingApiHandler eagerly.
 *
 * <p>All of these methods can be called on any thread.
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingApiBridge {
    private static final String TAG = "SBApiBridge";
    private static final boolean DEBUG = false;
    // A fake callback id used to init the Safe Browsing API.
    private static final long CALLBACK_ID_FOR_STARTUP = -1;

    private static final Object sSafetyNetApiHandlerLock = new Object();

    private static final Object sSafeBrowsingApiHandlerLock = new Object();

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static boolean sSafetyNetApiHandlerInitCalled;

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static SafetyNetApiHandler sSafetyNetApiHandler;

    @GuardedBy("sSafeBrowsingApiHandlerLock")
    private static SafeBrowsingApiHandler sSafeBrowsingApiHandler;

    @GuardedBy("sSafeBrowsingApiHandlerLock")
    private static UrlCheckTimeObserver sSafeBrowsingApiUrlCheckTimeObserver;

    private SafeBrowsingApiBridge() {
        // Util class, do not instantiate.
    }

    /**
     * Sets the {@link SafetyNetApiHandler} object once and for the lifetime of this process.
     *
     * @param handler An instance that has not been initialized.
     */
    public static void setSafetyNetApiHandler(SafetyNetApiHandler handler) {
        synchronized (sSafetyNetApiHandlerLock) {
            assert sSafetyNetApiHandler == null;
            sSafetyNetApiHandler = handler;
        }
    }

    /**
     * Sets the {@link SafeBrowsingApiHandler} object once and for the lifetime of this process.
     *
     * @param handler The handler that is injected when the process starts.
     */
    public static void setSafeBrowsingApiHandler(SafeBrowsingApiHandler handler) {
        synchronized (sSafeBrowsingApiHandlerLock) {
            assert sSafeBrowsingApiHandler == null;
            sSafeBrowsingApiHandler = handler;
            sSafeBrowsingApiHandler.setObserver(new SafeBrowsingApiLookupDoneObserver());
        }
    }

    /**
     * Clear the handlers to prepare for the next run.
     * This is needed for native to Java bridge test because the bridge is not destroyed between
     * each tests.
     */
    public static void clearHandlerForTesting() {
        synchronized (sSafetyNetApiHandlerLock) {
            sSafetyNetApiHandlerInitCalled = false;
            sSafetyNetApiHandler = null;
        }
        synchronized (sSafeBrowsingApiHandlerLock) {
            sSafeBrowsingApiHandler = null;
        }
    }

    /**
     * Initializes the singleton SafetyNetApiHandler instance on the first call. On subsequent calls
     * it does nothing, returns the same value as returned on the first call.
     *
     * <p>The caller must call {@link #setSafetyNetApiHandler(SafetyNetApiHandler)} first.
     *
     * @return true iff the initialization succeeded.
     */
    @CalledByNative
    public static boolean ensureSafetyNetApiInitialized() {
        synchronized (sSafetyNetApiHandlerLock) {
            return getSafetyNetApiHandler() != null;
        }
    }

    /**
     * Make a fake lookupUri request to the Safe Browsing API to accelerate subsequent calls. It is
     * optional to call this function before sending a real request.
     *
     * <p>The caller must call {@link #setSafeBrowsingApiHandler(SafeBrowsingApiHandler)} first.
     */
    public static void initSafeBrowsingApi() {
        // Use an empty URL so GMSCore won't send a real request to Safe Browsing.
        startUriLookupBySafeBrowsingApi(CALLBACK_ID_FOR_STARTUP, "", new int[0], 0);
    }

    /** Observer to record latency from requests to GmsCore. */
    public interface UrlCheckTimeObserver {
        /**
         * @param urlCheckTimeDeltaMicros Time it took for {@link SafetyNetApiHandler} to check the
         *     URL.
         */
        void onUrlCheckTime(long urlCheckTimeDeltaMicros);
    }

    /**
     * Set the observer to notify about the time it took to respond for SafeBrowsing response via
     * SafeBrowsing API. Notified for the first URL check, and only once.
     *
     * @param observer the observer to notify.
     */
    public static void setOneTimeSafeBrowsingApiUrlCheckObserver(UrlCheckTimeObserver observer) {
        synchronized (sSafeBrowsingApiHandlerLock) {
            sSafeBrowsingApiUrlCheckTimeObserver = observer;
        }
    }

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static SafetyNetApiHandler getSafetyNetApiHandler() {
        if (!sSafetyNetApiHandlerInitCalled) {
            sSafetyNetApiHandler = initSafetyNetApiHandler();
            sSafetyNetApiHandlerInitCalled = true;
        }
        return sSafetyNetApiHandler;
    }

    /**
     * Initializes the SafetyNetApiHandler, if supported.
     *
     * The caller must {@link #setSafetyNetApiHandler(SafetyNetApiHandler)} first.
     *
     * @return the handler if it is usable, or null if the API is not supported.
     */
    @GuardedBy("sSafetyNetApiHandlerLock")
    private static SafetyNetApiHandler initSafetyNetApiHandler() {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.initSafetyNetApiHandler")) {
            if (DEBUG) {
                Log.i(TAG, "initSafetyNetApiHandler");
            }
            if (sSafetyNetApiHandler == null) return null;
            return sSafetyNetApiHandler.init(new SafetyNetApiLookupDoneObserver())
                    ? sSafetyNetApiHandler
                    : null;
        }
    }

    private static class SafetyNetApiLookupDoneObserver implements SafetyNetApiHandler.Observer {
        @Override
        public void onVerifyAppsEnabledDone(long callbackId, int result) {
            synchronized (sSafetyNetApiHandlerLock) {
                SafeBrowsingApiBridgeJni.get().onVerifyAppsEnabledDone(callbackId, result);
            }
        }
    }

    private static class SafeBrowsingApiLookupDoneObserver
            implements SafeBrowsingApiHandler.Observer {
        @Override
        public void onUrlCheckDone(
                long callbackId,
                @LookupResult int lookupResult,
                int threatType,
                int[] threatAttributes,
                int responseStatus,
                long checkDelta) {
            if (callbackId == CALLBACK_ID_FOR_STARTUP) {
                // Not delivering the callback result to native if this is the call for startup. The
                // native library may not be ready, and there is no one on the native side listening
                // to the call for startup anyway.
                return;
            }
            synchronized (sSafeBrowsingApiHandlerLock) {
                if (sSafeBrowsingApiUrlCheckTimeObserver != null) {
                    sSafeBrowsingApiUrlCheckTimeObserver.onUrlCheckTime(checkDelta);
                    sSafeBrowsingApiUrlCheckTimeObserver = null;
                }
                SafeBrowsingApiBridgeJni.get()
                        .onUrlCheckDoneBySafeBrowsingApi(
                                callbackId,
                                lookupResult,
                                threatType,
                                threatAttributes,
                                responseStatus,
                                checkDelta);
            }
        }
    }

    /**
     * Starts a Safe Browsing Allowlist check.
     *
     * <p>Must only be called if {@link #ensureSafetyNetApiInitialized()} returns true.
     *
     * @return true iff the uri is in the allowlist.
     */
    @CalledByNative
    private static boolean startAllowlistLookup(String uri, int threatType) {
        synchronized (sSafetyNetApiHandlerLock) {
            assert sSafetyNetApiHandlerInitCalled;
            assert sSafetyNetApiHandler != null;
            try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.startAllowlistLookup")) {
                return getSafetyNetApiHandler().startAllowlistLookup(uri, threatType);
            }
        }
    }

    /**
     * Starts a Safe Browsing check through SafeBrowsing API.
     *
     */
    @CalledByNative
    private static void startUriLookupBySafeBrowsingApi(
            long callbackId, String uri, int[] threatTypes, int protocol) {
        synchronized (sSafeBrowsingApiHandlerLock) {
            if (sSafeBrowsingApiHandler == null) {
                // sSafeBrowsingApiHandler can only be null in tests.
                // Not delivering the callback result to native if this is the call for startup. The
                // native library may not be ready, and there is no one on the native side listening
                // to the call for startup anyway.
                // This is handled the same way as in onUrlCheckDone.
                if (callbackId != CALLBACK_ID_FOR_STARTUP) {
                    SafeBrowsingApiBridgeJni.get()
                            .onUrlCheckDoneBySafeBrowsingApi(
                                    callbackId,
                                    LookupResult.FAILURE_HANDLER_NULL,
                                    0,
                                    new int[0],
                                    0,
                                    0);
                }
                return;
            }
            sSafeBrowsingApiHandler.startUriLookup(callbackId, uri, threatTypes, protocol);
        }
    }

    /**
     * Check if app verification is enabled through the SafetyNet API.
     *
     * <p>Must only be called if {@link #ensureSafetyNetApiInitialized()} returns true.
     */
    @CalledByNative
    public static void isVerifyAppsEnabled(long callbackId) {
        synchronized (sSafetyNetApiHandlerLock) {
            assert sSafetyNetApiHandlerInitCalled;
            assert sSafetyNetApiHandler != null;
            sSafetyNetApiHandler.isVerifyAppsEnabled(callbackId);
        }
    }

    /**
     * Prompt the user to enable app verification through the SafetyNet API.
     *
     * <p>Must only be called if {@link #ensureSafetyNetApiInitialized()} returns true.
     */
    @CalledByNative
    public static void enableVerifyApps(long callbackId) {
        synchronized (sSafetyNetApiHandlerLock) {
            assert sSafetyNetApiHandlerInitCalled;
            assert sSafetyNetApiHandler != null;
            sSafetyNetApiHandler.enableVerifyApps(callbackId);
        }
    }

    @NativeMethods
    interface Natives {
        void onUrlCheckDoneBySafeBrowsingApi(
                long callbackId,
                int lookupResult,
                int threatType,
                int[] threatAttributes,
                int responseStatus,
                long checkDelta);

        void onVerifyAppsEnabledDone(long callbackId, int result);
    }
}
