// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.lang.reflect.InvocationTargetException;

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

    // Volatile to allow it being set from any thread.
    private static volatile SafeBrowsingApiHandler sHandler;
    private static UrlCheckTimeObserver sUrlCheckTimeObserver;

    // Deprecated. See setHandler().
    private static Class<? extends SafeBrowsingApiHandler> sHandlerClass;

    private SafeBrowsingApiBridge() {
        // Util class, do not instantiate.
    }

    /**
     * Set the class-file for the implementation of SafeBrowsingApiHandler to use when the safe
     * browsing api is invoked.
     *
     * TODO(crbug.com/1296097): Remove this method when all clients are switched to
     * {@link #setHandler(SafeBrowsingApiHandler)}.
     */
    @Deprecated
    public static void setSafeBrowsingHandlerType(
            Class<? extends SafeBrowsingApiHandler> handlerClass) {
        sHandlerClass = handlerClass;
    }

    /**
     * Sets the {@link SafeBrowsingApiHandler} object once and for the lifetime of this process.
     *
     * @param handler An instance that has not been initialized.
     */
    public static void setHandler(SafeBrowsingApiHandler handler) {
        assert sHandler == null;
        sHandler = handler;
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
        return getHandler() != null;
    }

    private static SafeBrowsingApiHandler getHandler() {
        return LazyHolder.INSTANCE;
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
     * The notification happens on another thread. The caller *must* guarantee that setting the
     * observer happens-before (in JMM sense) the first SafeBrowsing request is made.
     *
     * @param observer the observer to notify.
     */
    public static void setOneTimeUrlCheckObserver(UrlCheckTimeObserver observer) {
        sUrlCheckTimeObserver = observer;
    }

    private static class LazyHolder {
        static final SafeBrowsingApiHandler INSTANCE = initHandler();
    }

    /**
     * Initializes the SafeBrowsingApiHandler, if supported.
     *
     * The caller must {@link #setHandler(SafeBrowsingApiHandler)} first.
     *
     * @return the handler if it is usable, or null if the API is not supported.
     */
    private static SafeBrowsingApiHandler initHandler() {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.initHandler")) {
            return initHandlerInTraceEvent();
        }
    }

    private static SafeBrowsingApiHandler initHandlerInTraceEvent() {
        if (DEBUG) {
            Log.i(TAG, "initHandler");
        }
        SafeBrowsingApiHandler handler;
        if (sHandler != null) {
            handler = sHandler;
        } else {
            try {
                handler = sHandlerClass.getDeclaredConstructor().newInstance();
            } catch (NullPointerException | InstantiationException | IllegalAccessException
                    | NoSuchMethodException | InvocationTargetException e) {
                Log.e(TAG, "Failed to init handler: " + e.getMessage());
                return null;
            }
        }
        boolean initSuccessful = handler.init((callbackId, resultStatus, metadata, checkDelta) -> {
            if (sUrlCheckTimeObserver != null) {
                sUrlCheckTimeObserver.onUrlCheckTime(checkDelta);
                TraceEvent.instant("FirstSafeBrowsingResponse", String.valueOf(checkDelta));
                sUrlCheckTimeObserver = null;
            }
            SafeBrowsingApiBridgeJni.get().onUrlCheckDone(
                    callbackId, resultStatus, metadata, checkDelta);
        });
        return initSuccessful ? handler : null;
    }

    /**
     * Starts a Safe Browsing check.
     */
    @CalledByNative
    private static void startUriLookup(long callbackId, String uri, int[] threatsOfInterest) {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.startUriLookup")) {
            assert getHandler() != null;
            if (DEBUG) {
                Log.i(TAG, "Starting request: %s", uri);
            }
            getHandler().startUriLookup(callbackId, uri, threatsOfInterest);
            if (DEBUG) {
                Log.i(TAG, "Done starting request: %s", uri);
            }
        }
    }

    /**
     * TODO(crbug.com/995926): Make this call async
     * Starts a Safe Browsing Allowlist check.
     *
     * @return true iff the uri is in the allowlist.
     */
    @CalledByNative
    private static boolean startAllowlistLookup(String uri, int threatType) {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.startAllowlistLookup")) {
            assert getHandler() != null;
            return getHandler().startAllowlistLookup(uri, threatType);
        }
    }

    @NativeMethods
    interface Natives {
        void onUrlCheckDone(long callbackId, int resultStatus, String metadata, long checkDelta);
    }
}
