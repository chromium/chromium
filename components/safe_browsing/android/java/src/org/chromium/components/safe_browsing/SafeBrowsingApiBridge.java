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
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingApiBridge {
    private static final String TAG = "SBApiBridge";
    private static final boolean DEBUG = false;

    private static Class<? extends SafeBrowsingApiHandler> sHandlerClass;
    private static UrlCheckTimeObserver sUrlCheckTimeObserver;

    private SafeBrowsingApiBridge() {
        // Util class, do not instantiate.
    }

    /**
     * Set the class-file for the implementation of SafeBrowsingApiHandler to use when the safe
     * browsing api is invoked.
     */
    public static void setSafeBrowsingHandlerType(
            Class<? extends SafeBrowsingApiHandler> handlerClass) {
        if (DEBUG) {
            Log.i(TAG, "setSafeBrowsingHandlerType: " + String.valueOf(handlerClass != null));
        }
        sHandlerClass = handlerClass;
    }

    /**
     * Creates the singleton SafeBrowsingApiHandler instance on the first call. On subsequent calls
     * does nothing, returns the same value as returned on the first call.
     *
     * The caller must {@link #setSafeBrowsingHandlerType(Class)} first.
     *
     * @return true iff the creation succeeded.
     */
    @CalledByNative
    public static boolean ensureCreated() {
        return getHandler() != null;
    }

    // Lazily creates the singleton. Can be invoked from any thread.
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
        static final SafeBrowsingApiHandler INSTANCE = create();
    }

    /**
     * Creates a SafeBrowsingApiHandler and initialize its client, if supported.
     *
     * The caller must {@link #setSafeBrowsingHandlerType(Class)} first.
     *
     * @return the handler if it's usable, or null if the API is not supported.
     */
    private static SafeBrowsingApiHandler create() {
        try (TraceEvent t = TraceEvent.scoped("SafeBrowsingApiBridge.create")) {
            return createInTraceEvent();
        }
    }

    private static SafeBrowsingApiHandler createInTraceEvent() {
        if (DEBUG) {
            Log.i(TAG, "create");
        }
        SafeBrowsingApiHandler handler;
        try {
            handler = sHandlerClass.getDeclaredConstructor().newInstance();
        } catch (NullPointerException | InstantiationException | IllegalAccessException
                | NoSuchMethodException | InvocationTargetException e) {
            Log.e(TAG, "Failed to init handler: " + e.getMessage());
            return null;
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
