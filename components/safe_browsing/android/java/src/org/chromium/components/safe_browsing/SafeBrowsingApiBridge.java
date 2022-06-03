// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.chromium.base.Log;
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

    private static Class<? extends SafeBrowsingApiHandler> sHandler;

    private SafeBrowsingApiBridge() {
        // Util class, do not instantiate.
    }

    /**
     * Set the class-file for the implementation of SafeBrowsingApiHandler to use when the safe
     * browsing api is invoked.
     */
    public static void setSafeBrowsingHandlerType(Class<? extends SafeBrowsingApiHandler> handler) {
        sHandler = handler;
    }

    /**
     * Create a SafeBrowsingApiHandler obj and initialize its client, if supported.
     *
     * @return the handler if it's usable, or null if the API is not supported.
     */
    @CalledByNative
    private static SafeBrowsingApiHandler create() {
        SafeBrowsingApiHandler handler;
        try {
            handler = sHandler.getDeclaredConstructor().newInstance();
        } catch (NullPointerException | InstantiationException | IllegalAccessException
                | NoSuchMethodException | InvocationTargetException e) {
            Log.e(TAG, "Failed to init handler: " + e.getMessage());
            return null;
        }
        boolean initSuccesssful =
                handler.init(new SafeBrowsingApiHandler.Observer() {
                    @Override
                    public void onUrlCheckDone(
                            long callbackId, int resultStatus, String metadata, long checkDelta) {
                        SafeBrowsingApiBridgeJni.get().onUrlCheckDone(
                                callbackId, resultStatus, metadata, checkDelta);
                    }
                });
        return initSuccesssful ? handler : null;
    }

    /**
     * Starts a Safe Browsing check. Must be called on the same sequence as |create|.
     */
    @CalledByNative
    private static void startUriLookup(
            SafeBrowsingApiHandler handler, long callbackId, String uri, int[] threatsOfInterest) {
        if (DEBUG) {
            Log.i(TAG, "Starting request: %s", uri);
        }
        handler.startUriLookup(callbackId, uri, threatsOfInterest);
        if (DEBUG) {
            Log.i(TAG, "Done starting request: %s", uri);
        }
    }

    /**
     * TODO(crbug.com/995926): Make this call async
     * Starts a Safe Browsing Allowlist check.
     *
     * If the uri is in the allowlist, return true. Otherwise, return false.
     */
    @CalledByNative
    private static boolean startAllowlistLookup(
            SafeBrowsingApiHandler handler, String uri, int threatType) {
        return handler.startAllowlistLookup(uri, threatType);
    }

    @NativeMethods
    interface Natives {
        void onUrlCheckDone(long callbackId, int resultStatus, String metadata, long checkDelta);
    }
}
