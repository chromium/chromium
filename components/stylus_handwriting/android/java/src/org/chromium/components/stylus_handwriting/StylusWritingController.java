// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.os.Build;
import android.view.PointerIcon;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class to determine whether Direct writing service is in consideration or the Android
 * platform Stylus Writing feature, and to set the appropriate handler to WebContents.
 */
public class StylusWritingController {
    private final Context mContext;
    private WebContents mCurrentWebContents;
    @Nullable private PointerIcon mHandwritingIcon;
    private boolean mIconFetched;
    private boolean mLazyFetchHandWritingIconFeatureEnabled;
    private boolean mShouldOverrideStylusHoverIcon;
    private boolean mIsFocused;

    @Nullable private AndroidStylusWritingHandler mAndroidHandler;
    @Nullable private DirectWritingTrigger mDirectWritingTrigger;
    @Nullable private DisabledStylusWritingHandler mDisabledStylusWritingHandler;

    static StylusWritingController createControllerForTests(Context context, PointerIcon icon) {
        StylusWritingController controller = new StylusWritingController(context);
        controller.mIconFetched = true;
        controller.mHandwritingIcon = icon;
        return controller;
    }

    /** Creates a new instance of this class. */
    public StylusWritingController(Context context) {
        this(context, false);
    }

    public StylusWritingController(
            Context context, boolean lazyFetchHandWritingIconFeatureEnabled) {
        mContext = context;
        mLazyFetchHandWritingIconFeatureEnabled = lazyFetchHandWritingIconFeatureEnabled;
        mIconFetched = false;
        if (!mLazyFetchHandWritingIconFeatureEnabled) {
            int iconType = getHandler().getStylusPointerIcon();
            if (iconType != PointerIcon.TYPE_NULL) {
                mHandwritingIcon =
                        PointerIcon.getSystemIcon(context, getHandler().getStylusPointerIcon());
            }
        }
    }

    private PointerIcon getHandwritingIcon() {
        if (!mIconFetched) {
            int iconType = getHandler().getStylusPointerIcon();
            if (iconType != PointerIcon.TYPE_NULL) {
                mHandwritingIcon = PointerIcon.getSystemIcon(mContext, iconType);
            }
            mIconFetched = true;
        }

        return mHandwritingIcon;
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && AndroidStylusWritingHandler.isEnabled(mContext)) {
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
        webContents
                .getViewAndroidDelegate()
                .setShouldShowStylusHoverIconCallback(this::setShouldOverrideStylusHoverIcon);
    }

    /**
     * Notify window focus has changed
     *
     * @param hasFocus whether window gained or lost focus
     */
    public void onWindowFocusChanged(boolean hasFocus) {
        // This notification is used to determine if the Stylus writing feature is enabled or not
        // from System settings as it can be changed while Chrome is in background.
        mIsFocused = hasFocus;
        updateStylusState();
    }

    /** Notify stylus related settings changed. */
    public void onSettingsChange() {
        updateStylusState();
    }

    @Nullable
    public PointerIcon resolvePointerIcon() {
        if (mShouldOverrideStylusHoverIcon) {
            return mLazyFetchHandWritingIconFeatureEnabled
                    ? getHandwritingIcon()
                    : mHandwritingIcon;
        }
        return null;
    }

    private void setShouldOverrideStylusHoverIcon(boolean shouldOverride) {
        mShouldOverrideStylusHoverIcon = shouldOverride;
    }

    private void updateStylusState() {
        StylusApiOption handler = getHandler();
        // TODO(crbug.com/372430494): Refactor handler to have a dedicated method to update state
        // instead of calling onWindowFocusChanged.
        handler.onWindowFocusChanged(mContext, mIsFocused);

        if (mCurrentWebContents == null) return;
        handler.onWebContentsChanged(mContext, mCurrentWebContents);
        if (mCurrentWebContents.getViewAndroidDelegate() == null) return;
        mCurrentWebContents
                .getViewAndroidDelegate()
                .setShouldShowStylusHoverIconCallback(this::setShouldOverrideStylusHoverIcon);
    }
}
