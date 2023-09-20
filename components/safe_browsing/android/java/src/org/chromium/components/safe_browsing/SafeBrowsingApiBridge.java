// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.GuardedBy;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler.LookupResult;

/**
 * Helper for calling GMSCore APIs from native code to perform Safe Browsing checks.
 *
 * {@link #setSafetyNetApiHandler(SafetyNetApiHandler)} and {@link
 * #setSafeBrowsingApiHandler(SafeBrowsingApiHandler)} must be invoked first. After that
 * {@link #startUriLookupBySafetyNetApi(long, String, int[])}, {@link
 * #startUriLookupBySafeBrowsingApi(long, String, int[], int)} and {@link
 * #startAllowlistLookup(String, int)} can be used to check the URLs. The SafetyNetApiHandler is
 * initialized lazily on the first URL check. There is no extra step needed to initialize the
 * SafeBrowsingApiHandler.
 *
 * Optionally calling {@link #ensureSafetyNetApiInitialized()} allows to initialize the
 * SafetyNetApiHandler eagerly.
 *
 * All of these methods can be called on any thread.
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingApiBridge {
    private static final String TAG = "SBApiBridge";
    private static final boolean DEBUG = false;

    private static final Object sSafetyNetApiHandlerLock = new Object();

    private static final Object sSafeBrowsingApiHandlerLock = new Object();

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static boolean sSafetyNetApiHandlerInitCalled;

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static SafetyNetApiHandler sSafetyNetApiHandler;

    @GuardedBy("sSafetyNetApiHandlerLock")
    private static UrlCheckTimeObserver sSafetyNetApiUrlCheckTimeObserver;

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
     * Initializes the singleton SafetyNetApiHandler instance on the first call. On subsequent
     * calls it does nothing, returns the same value as returned on the first call.
     *
     * The caller must {@link #setSafetyNetApiHandler(SafetyNetApiHandler)} first.
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
     * Observer to record latency from requests to GmsCore.
     */
    public interface UrlCheckTimeObserver {
        /**
         * @param urlCheckTimeDeltaMicros Time it took for {@link SafetyNetApiHandler} to check
         * the URL.
         */
        void onUrlCheckTime(long urlCheckTimeDeltaMicros);
    }

    /**
     * Set the observer to notify about the time it took to respond for SafeBrowsing response via
     * SafetyNet API. Notified for the first URL check, and only once.
     *
     * @param observer the observer to notify.
     */
    public static void setOneTimeSafetyNetApiUrlCheckObserver(UrlCheckTimeObserver observer) {
        synchronized (sSafetyNetApiHandlerLock) {
            sSafetyNetApiUrlCheckTimeObserver = observer;
        }
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
        public void onUrlCheckDone(
                long callbackId, int resultStatus, String metadata, long checkDelta) {
            synchronized (sSafetyNetApiHandlerLock) {
                if (DEBUG) {
                    Log.i(TAG,
                            "onUrlCheckDone resultStatus=" + resultStatus
                                    + ", metadata=" + metadata);
                }
                if (sSafetyNetApiUrlCheckTimeObserver != null) {
                    sSafetyNetApiUrlCheckTimeObserver.onUrlCheckTime(checkDelta);
                    TraceEvent.instant("FirstSafeBrowsingResponseFromSafetyNetApi",
                            String.valueOf(checkDelta));
                    sSafetyNetApiUrlCheckTimeObserver = null;
                }
                SafeBrowsingApiBridgeJni.get().onUrlCheckDoneBySafetyNetApi(
                        callbackId, resultStatus, metadata, checkDelta);
            }
        }
    }

    private static class SafeBrowsingApiLookupDoneObserver
            implements SafeBrowsingApiHandler.Observer {
        @Override
        public void onUrlCheckDone(long callbackId, @LookupResult int lookupResult, int threatType,
                int[] threatAttributes, int responseStatus, long checkDelta) {
            synchronized (sSafeBrowsingApiHandlerLock) {
                if (sSafeBrowsingApiUrlCheckTimeObserver != null) {
                    sSafeBrowsingApiUrlCheckTimeObserver.onUrlCheckTime(checkDelta);
                    sSafeBrowsingApiUrlCheckTimeObserver = null;
                }
                SafeBrowsingApiBridgeJni.get().onUrlCheckDoneBySafeBrowsingApi(callbackId,
                        lookupResult, threatType, threatAttributes, responseStatus, checkDelta);
            }
        }
    }

    /**
     * Starts a Safe Browsing check through SafetyNet API.
     *
     * Must only be called if {@link #ensureSafetyNetApiInitialized()} returns true.
     */
    @CalledByNative
    private static void startUriLookupBySafetyNetApi(
            long callbackId, String uri, int[] threatsOfInterest) {
        synchronized (sSafetyNetApiHandlerLock) {
            assert sSafetyNetApiHandlerInitCalled;
            assert sSafetyNetApiHandler != null;
            try (TraceEvent t = TraceEvent.scoped(
                         "SafeBrowsingApiBridge.startUriLookupBySafetyNetApi")) {
                if (DEBUG) {
                    Log.i(TAG, "Starting request: %s", uri);
                }
                getSafetyNetApiHandler().startUriLookup(callbackId, uri, threatsOfInterest);
                if (DEBUG) {
                    Log.i(TAG, "Done starting request: %s", uri);
                }
            }
        }
    }

    /**
     * Starts a Safe Browsing Allowlist check.
     *
     * Must only be called if {@link #ensureSafetyNetApiInitialized()} returns true.
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
            assert sSafeBrowsingApiHandler != null;
            sSafeBrowsingApiHandler.startUriLookup(callbackId, uri, threatTypes, protocol);
        }
    }

    @NativeMethods
    interface Natives {
        void onUrlCheckDoneBySafetyNetApi(
                long callbackId, int resultStatus, String metadata, long checkDelta);
        void onUrlCheckDoneBySafeBrowsingApi(long callbackId, int lookupResult, int threatType,
                int[] threatAttributes, int responseStatus, long checkDelta);
    }
}
