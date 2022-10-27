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

/**
 * Helper for calling GMSCore Safe Browsing API from native code.
 *
 * The {@link #setHandler(SafeBrowsingApiHandler)} must be invoked first. After that
 * {@link #startUriLookup(long, String, int[])} and {@link #startAllowlistLookup(String, int)} can
 * be used to check the URLs. The handler would be initialized lazily on the first URL check.
 *
 * Optionally calling {@link #ensureInitialized()} allows to initialize the handler eagerly.
 *
 * All of these methods can be called on any thread.
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingApiBridge {
    private static final String TAG = "SBApiBridge";
    private static final boolean DEBUG = false;

    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static boolean sHandlerInitCalled;

    @GuardedBy("sLock")
    private static SafeBrowsingApiHandler sHandler;

    @GuardedBy("sLock")
    private static UrlCheckTimeObserver sUrlCheckTimeObserver;

    private SafeBrowsingApiBridge() {
        // Util class, do not instantiate.
    }

    /**
     * Sets the {@link SafeBrowsingApiHandler} object once and for the lifetime of this process.
     *
     * @param handler An instance that has not been initialized.
     */
    public static void setHandler(SafeBrowsingApiHandler handler) {
        synchronized (sLock) {
            assert sHandler == null;
            sHandler = handler;
        }
    }

    /**
     * Initializes the singleton SafeBrowsingApiHandler instance on the first call. On subsequent
     * calls it does nothing, returns the same value as returned on the first call.
     *
     * The caller must {@link #setHandler(SafeBrowsingApiHandler)} first.
     *
     * @return true iff the initialization succeeded.
     */
    @CalledByNative
    public static boolean ensureInitialized() {
        synchronized (sLock) {
            return getHandler() != null;
        }
    }

    /**
     * Observer to record latency from requests to GmsCore.
     */
    public interface UrlCheckTimeObserver {
        /**
         * @param urlCheckTimeDeltaMicros Time it took for {@link SafeBrowsingApiHandler} to check
         * the URL.
         */
        void onUrlCheckTime(long urlCheckTimeDeltaMicros);
    }

    /**
     * Set the observer to notify about the time it took to respond for SafeBrowsing. Notified for
     * the first URL check, and only once.
     *
     * @param observer the observer to notify.
     */
    public static void setOneTimeUrlCheckObserver(UrlCheckTimeObserver observer) {
        synchronized (sLock) {
            sUrlCheckTimeObserver = observer;
        }
    }

    @GuardedBy("sLock")
    private static SafeBrowsingApiHandler getHandler() {
        if (!sHandlerInitCalled) {
            sHandler = initHandler();
            sHandlerInitCalled = true;
        }
        return sHandler;
    }

    /**
     * Initializes the SafeBrowsingApiHandler, if supported.
     *
     * The caller must {@link #setHandler(SafeBrowsingApiHandler)} first.
     *
     * @return the handler if it is usable, or null if the API is not supported.
     */
    @GuardedBy("sLock")
    private static SafeBrowsingApiHandler initHandler() {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.initHandler")) {
            if (DEBUG) {
                Log.i(TAG, "initHandler");
            }
            if (sHandler == null) return null;
            return sHandler.init(new LookupDoneObserver()) ? sHandler : null;
        }
    }

    private static class LookupDoneObserver implements SafeBrowsingApiHandler.Observer {
        @Override
        public void onUrlCheckDone(
                long callbackId, int resultStatus, String metadata, long checkDelta) {
            synchronized (sLock) {
                if (DEBUG) {
                    Log.i(TAG,
                            "onUrlCheckDone resultStatus=" + resultStatus
                                    + ", metadata=" + metadata);
                }
                if (sUrlCheckTimeObserver != null) {
                    sUrlCheckTimeObserver.onUrlCheckTime(checkDelta);
                    TraceEvent.instant("FirstSafeBrowsingResponse", String.valueOf(checkDelta));
                    sUrlCheckTimeObserver = null;
                }
                SafeBrowsingApiBridgeJni.get().onUrlCheckDone(
                        callbackId, resultStatus, metadata, checkDelta);
            }
        }
    }

    /**
     * Starts a Safe Browsing check.
     *
     * Must only be called if {@link #ensureInitialized()} returns true.
     */
    @CalledByNative
    private static void startUriLookup(long callbackId, String uri, int[] threatsOfInterest) {
        synchronized (sLock) {
            assert sHandlerInitCalled;
            assert sHandler != null;
            try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.startUriLookup")) {
                if (DEBUG) {
                    Log.i(TAG, "Starting request: %s", uri);
                }
                getHandler().startUriLookup(callbackId, uri, threatsOfInterest);
                if (DEBUG) {
                    Log.i(TAG, "Done starting request: %s", uri);
                }
            }
        }
    }

    /**
     * Starts a Safe Browsing Allowlist check.
     *
     * Must only be called if {@link #ensureInitialized()} returns true.
     *
     * @return true iff the uri is in the allowlist.
     *
     * TODO(crbug.com/995926): Make this call async.
     */
    @CalledByNative
    private static boolean startAllowlistLookup(String uri, int threatType) {
        synchronized (sLock) {
            assert sHandlerInitCalled;
            assert sHandler != null;
            try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.startAllowlistLookup")) {
                return getHandler().startAllowlistLookup(uri, threatType);
            }
        }
    }

    @NativeMethods
    interface Natives {
        void onUrlCheckDone(long callbackId, int resultStatus, String metadata, long checkDelta);
    }
}
