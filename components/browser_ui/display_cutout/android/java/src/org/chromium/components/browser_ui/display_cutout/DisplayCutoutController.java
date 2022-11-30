// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.display_cutout;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controls the display safe area for a {@link WebContents} and the cutout mode for an {@link
 * Activity} window.
 *
 * The WebContents is updated with the safe area continuously, as long as {@link
 * Delegate#getAttachedActivity()} returns a non-null value. The cutout mode is set on the
 * Activity's window only in P+, and only when the associated WebContents is fullscreen.
 */
public class DisplayCutoutController implements InsetObserverView.WindowInsetObserver {
    /** {@link Window} of the current {@link Activity}. */
    private Window mWindow;

    /** The current viewport fit value. */
    private @WebContentsObserver.ViewportFitType int mViewportFit = ViewportFit.AUTO;

    /**
     * The current {@link InsetObserverView} that we are attached to. This can be null if we
     * have not attached to an activity.
     */
    private @Nullable InsetObserverView mInsetObserverView;

    /**
     * Provides the activity-specific (vs tab-specific) cutout mode. The activity-specific
     * cutout mode takes precedence over the tab-specific cutout mode.
     */
    private @Nullable ObservableSupplier<Integer> mBrowserCutoutModeSupplier;

    /**
     * Observes {@link mBrowserCutoutModeSupplier}.
     */
    private @Nullable Callback<Integer> mBrowserCutoutModeObserver;

    /** An interface for providing embedder-specific behavior to the controller. */
    public interface Delegate {
        /** Returns the activity this controller is associated with, if there is one. */
        @Nullable
        Activity getAttachedActivity();

        /**
         * Returns the {@link WebContents} this controller should update the safe area for, if
         * there is one.
         */
        @Nullable
        WebContents getWebContents();

        /** Returns the view this controller uses for safe area updates, if there is one. */
        @Nullable
        InsetObserverView getInsetObserverView();

        /** Returns whether the user can interact with the associated WebContents/UI element. */
        boolean isInteractable();

        /**
         * Returns the activity-specific (vs tab-specific) cutout mode. The activity-specific
         * cutout mode takes precedence over the tab-specific cutout mode.
         */
        ObservableSupplier<Integer> getBrowserDisplayCutoutModeSupplier();

        /** Whether the activity is in browser (not-HTML) fullscreen. */
        boolean isInBrowserFullscreen();
    }
    private final Delegate mDelegate;

    public DisplayCutoutController(Delegate delegate) {
        mDelegate = delegate;
        maybeAddObservers();
    }

    /**
     * Add observers to {@link InsetObserverView} and the browser display cutout mode supplier if we
     * have not added them.
     */
    void maybeAddObservers() {
        Activity activity = mDelegate.getAttachedActivity();
        if (activity == null) return;

        updateInsetObserver(mDelegate.getInsetObserverView());
        updateBrowserCutoutObserver(mDelegate.getBrowserDisplayCutoutModeSupplier());
        mWindow = activity.getWindow();
    }

    /** Remove observers added by {@link #maybeAddObservers()}. */
    void removeObservers() {
        updateInsetObserver(null);
        updateBrowserCutoutObserver(null);
        mWindow = null;
    }

    private void updateInsetObserver(InsetObserverView observer) {
        if (mInsetObserverView == observer) return;

        if (mInsetObserverView != null) {
            mInsetObserverView.removeObserver(this);
        }
        mInsetObserverView = observer;
        if (mInsetObserverView != null) {
            mInsetObserverView.addObserver(this);
        }
    }

    private void updateBrowserCutoutObserver(ObservableSupplier<Integer> supplier) {
        if (mBrowserCutoutModeSupplier == supplier) return;

        if (mBrowserCutoutModeObserver != null) {
            mBrowserCutoutModeSupplier.removeObserver(mBrowserCutoutModeObserver);
        }
        mBrowserCutoutModeSupplier = supplier;
        mBrowserCutoutModeObserver = null;
        if (mBrowserCutoutModeSupplier != null) {
            mBrowserCutoutModeObserver = (browserDisplayCutoutMode) -> {
                maybeUpdateLayout();
            };
            mBrowserCutoutModeSupplier.addObserver(mBrowserCutoutModeObserver);
        }
    }

    public void destroy() {
        removeObservers();
    }

    /**
     * Set the viewport fit value for the tab.
     * @param value The new viewport fit value.
     */
    public void setViewportFit(@WebContentsObserver.ViewportFitType int value) {
        if (value != ViewportFit.AUTO) {
            assert mDelegate.getWebContents().isFullscreenForCurrentTab()
                    || mDelegate.isInBrowserFullscreen();
        }

        if (value == mViewportFit) return;

        mViewportFit = value;
        maybeUpdateLayout();
    }

    /** Implements {@link WindowInsetsObserver}. */
    @Override
    public void onSafeAreaChanged(Rect area) {
        WebContents webContents = mDelegate.getWebContents();
        if (webContents == null) return;

        float dipScale = getDipScale();
        area.set(adjustInsetForScale(area.left, dipScale), adjustInsetForScale(area.top, dipScale),
                adjustInsetForScale(area.right, dipScale),
                adjustInsetForScale(area.bottom, dipScale));

        webContents.setDisplayCutoutSafeArea(area);
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {}

    /**
     * Adjusts a WindowInset inset to a CSS pixel value.
     * @param inset The inset as an integer.
     * @param dipScale The devices dip scale as an integer.
     * @return The CSS pixel value adjusted for scale.
     */
    private static int adjustInsetForScale(int inset, float dipScale) {
        return (int) Math.ceil(inset / dipScale);
    }

    @VisibleForTesting
    protected float getDipScale() {
        return mDelegate.getWebContents().getTopLevelNativeWindow().getDisplay().getDipScale();
    }

    /**
     * Converts a {@link ViewportFit} value into the Android P+ equivalent.
     * @returns String containing the {@link LayoutParams} field name of the
     *     equivalent value.
     */
    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.P)
    public int computeDisplayCutoutMode() {
        // If we are not interactable then force the default mode.
        if (!mDelegate.isInteractable()) {
            return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        }

        if (mBrowserCutoutModeSupplier != null) {
            int browserCutoutMode = mBrowserCutoutModeSupplier.get();
            if (browserCutoutMode != LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT) {
                return browserCutoutMode;
            }
        }

        switch (mViewportFit) {
            case ViewportFit.CONTAIN:
                return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
            case ViewportFit.COVER_FORCED_BY_USER_AGENT:
            case ViewportFit.COVER:
                return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            case ViewportFit.AUTO:
            default:
                return LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        }
    }

    @VisibleForTesting
    protected LayoutParams getWindowAttributes() {
        return mWindow == null ? null : mWindow.getAttributes();
    }

    @VisibleForTesting
    protected void setWindowAttributes(LayoutParams attributes) {
        mWindow.setAttributes(attributes);
    }

    /** Should be called to refresh the activity window's layout based on current state. */
    public void maybeUpdateLayout() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        LayoutParams attributes = getWindowAttributes();
        if (attributes == null) return;

        final int displayCutoutMode = computeDisplayCutoutMode();
        if (attributes.layoutInDisplayCutoutMode == displayCutoutMode) return;

        attributes.layoutInDisplayCutoutMode = displayCutoutMode;
        setWindowAttributes(attributes);
    }

    /** Should be called when the associated UI surface is attached or detached to an activity. */
    public void onActivityAttachmentChanged(@Nullable WindowAndroid window) {
        if (window == null) {
            removeObservers();
        } else {
            maybeAddObservers();
        }
    }
}
