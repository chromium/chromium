// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class to determine whether Direct writing service is in consideration or the Android
 * platform Stylus Writing feature, and to set the appropriate handler to WebContents.
 */
public class StylusWritingController {
    private final Context mContext;
    private WebContents mCurrentWebContents;

    @Nullable
    private AndroidStylusWritingHandler mAndroidHandler;
    @Nullable
    private DirectWritingTrigger mDirectWritingTrigger;
    @Nullable
    private DisabledStylusWritingHandler mDisabledStylusWritingHandler;

    /**
     * Creates a new instance of this class.
     */
    public StylusWritingController(Context context) {
        mContext = context;
    }

    /**
     * Returns the appropriate StylusWritingHandler - this may change at runtime if the user
     * enables/disables the stylus writing feature in their Android settings.
     */
    private StylusApiOption getHandler() {
        if (DirectWritingSettingsHelper.isEnabled(mContext)) {
            // Lazily initialize the various handlers since a lot of the time only one will be used.
            if (mDirectWritingTrigger == null) {
                mDirectWritingTrigger = new DirectWritingTrigger();
            }

            return mDirectWritingTrigger;
        }

        // The check for Android T is already in isEnabled but we are adding it here too to make
        // lint happy.
        if (BuildInfo.isAtLeastT() && AndroidStylusWritingHandler.isEnabled(mContext)) {
            if (mAndroidHandler == null) {
                mAndroidHandler = new AndroidStylusWritingHandler(mContext);
            }

            return mAndroidHandler;
        }

        if (mDisabledStylusWritingHandler == null) {
            mDisabledStylusWritingHandler = new DisabledStylusWritingHandler();
        }

        return mDisabledStylusWritingHandler;
    }

    /**
     * Notifies the applicable {@link StylusWritingHandler} so that stylus writing messages can be
     * received and handled for performing handwriting recognition.
     *
     * @param webContents current web contents
     */
    public void onWebContentsChanged(WebContents webContents) {
        if (webContents.getViewAndroidDelegate() == null) return;

        mCurrentWebContents = webContents;
        StylusApiOption handler = getHandler();
        handler.onWebContentsChanged(mContext, webContents);
        webContents.getViewAndroidDelegate().setStylusWritingCursorHandler(
                handler.getStylusWritingCursorHandler());
    }

    /**
     * Notify window focus has changed
     *
     * @param hasFocus whether window gained or lost focus
     */
    public void onWindowFocusChanged(boolean hasFocus) {
        // This notification is used to determine if the Stylus writing feature is enabled or not
        // from System settings as it can be changed while Chrome is in background.
        StylusApiOption handler = getHandler();
        handler.onWindowFocusChanged(mContext, hasFocus);

        if (mCurrentWebContents == null) return;
        handler.onWebContentsChanged(mContext, mCurrentWebContents);
        if (mCurrentWebContents.getViewAndroidDelegate() == null) return;
        mCurrentWebContents.getViewAndroidDelegate().setStylusWritingCursorHandler(
                handler.getStylusWritingCursorHandler());
    }
}
