// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.reflect.InvocationTargetException;

/**
 * Helper for calling GMSCore Safe Browsing API from native code.
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingApiBridge {
    private static final String TAG = "ApiBridge";

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
                        nativeOnUrlCheckDone(callbackId, resultStatus, metadata, checkDelta);
                    }
                }, nativeAreLocalBlacklistsEnabled());
        return initSuccesssful ? handler : null;
    }

    /**
     * Starts a Safe Browsing check. Must be called on the same sequence as |create|.
     */
    @CalledByNative
    private static void startUriLookup(
            SafeBrowsingApiHandler handler, long callbackId, String uri, int[] threatsOfInterest) {
        handler.startUriLookup(callbackId, uri, threatsOfInterest);
        Log.d(TAG, "Done starting request");
    }

    private static native boolean nativeAreLocalBlacklistsEnabled();
    private static native void nativeOnUrlCheckDone(
            long callbackId, int resultStatus, String metadata, long checkDelta);
}
