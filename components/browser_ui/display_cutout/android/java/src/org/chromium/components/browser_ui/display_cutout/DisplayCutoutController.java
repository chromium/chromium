// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.display_cutout;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controls the display safe area for a {@link WebContents} and the cutout mode for an {@link
 * Activity} window.
 *
 * <p>The WebContents is updated with the safe area continuously, as long as {@link
 * Delegate#getAttachedActivity()} returns a non-null value. The cutout mode is set on the
 * Activity's window only in P+, and only when the associated WebContents is fullscreen.
 */
public class DisplayCutoutController implements InsetObserver.WindowInsetObserver, UserData {
    private static final String TAG = "E2E_DCController";

    private static final Class<DisplayCutoutController> USER_DATA_KEY =
            DisplayCutoutController.class;

    /** {@link Window} of the current {@link Activity}. */
    private Window mWindow;

    /** The current viewport fit value. */
    private @WebContentsObserver.ViewportFitType int mViewportFit = ViewportFit.AUTO;

    /**
     * The current {@link InsetObserver} that we are attached to. This can be null if we have not
     * attached to an activity.
     */
    private @Nullable InsetObserver mInsetObserver;

    /**
     * Provides the activity-specific (vs tab-specific) cutout mode. The activity-specific
     * cutout mode takes precedence over the tab-specific cutout mode.
     */
    private @Nullable ObservableSupplier<Integer> mBrowserCutoutModeSupplier;

    /**
     * Observes {@link mBrowserCutoutModeSupplier}.
     */
    private @Nullable Callback<Integer> mBrowserCutoutModeObserver;

    /** Tracks Safe Area Insets. */
    private final SafeAreaInsetsTrackerImpl mSafeAreaInsetsTracker;

    /**
     * An interface to track general changes to Safe Area Insets. TODO(https://crbug.com/1475820)
     * Develop beyond this minimal stub.
     */
    public interface SafeAreaInsetsTracker {

        /**
         * @return whether this Tracker was created for a web page set to Cover.
         */
        boolean isViewportFitCover();
    }

    /**
     * Tracks general changes to Safe Area Insets. TODO(https://crbug.com/1475820) Track the Notch
     * and bottom in a class in a separate file.
     */
    private static class SafeAreaInsetsTrackerImpl implements SafeAreaInsetsTracker {
        private boolean mIsViewportFitCover;

        /** Sets whether this Tracker was created for a web page set to Cover. */
        public void setIsViewportFitCover(boolean isViewportFitCover) {
            mIsViewportFitCover = isViewportFitCover;
        }

        @Override
        public boolean isViewportFitCover() {
            return mIsViewportFitCover;
        }
    }

    /** An interface for providing embedder-specific behavior to the controller. */
    public interface Delegate {

        /** Returns the activity this controller is associated with, if there is one. */
        @Nullable
        Activity getAttachedActivity();

        /**
         * Returns the {@link WebContents} this controller should update the safe area for, if there
         * is one.
         */
        @Nullable
        WebContents getWebContents();

        /** Returns the view this controller uses for safe area updates, if there is one. */
        @Nullable
        InsetObserver getInsetObserverView();

        /** Returns whether the user can interact with the associated WebContents/UI element. */
        boolean isInteractable();

        /**
         * Returns the activity-specific (vs tab-specific) cutout mode. The activity-specific cutout
         * mode takes precedence over the tab-specific cutout mode.
         */
        ObservableSupplier<Integer> getBrowserDisplayCutoutModeSupplier();

        /** Whether the activity is in browser (not-HTML) fullscreen. */
        boolean isInBrowserFullscreen();

        /** Whether the basic Feature for drawing Edge To Edge is enabled. */
        boolean isDrawEdgeToEdgeEnabled();
    }

    private final Delegate mDelegate;

    /**
     * Gets the DisplayCutoutController from the current {@link Tab} if there is one, otherwise
     * {@code null} is returned.
     */
    private static @Nullable DisplayCutoutController from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        return host.getUserData(USER_DATA_KEY);
    }

    /**
     * Gets the DisplayCutoutController from the current {@link Tab}, creating one if needed.
     *
     * @param tab The Tab to get the controller from.
     * @param delegate A delegate to embedder-specific behavior to the controller.
     */
    public static @NonNull DisplayCutoutController createForTab(Tab tab, Delegate delegate) {
        UserDataHost host = tab.getUserDataHost();
        DisplayCutoutController controller = host.getUserData(USER_DATA_KEY);
        return controller != null
                ? controller
                : host.setUserData(USER_DATA_KEY, new DisplayCutoutController(delegate));
    }

    /**
     * Constructs a controller for the Notch in the Display. Works with a {@code
     * DisplayCutoutTabHelper} to track changes to the Viewport for the current Tab and allow
     * drawing around the Notch and pushing Safe Area Insets back to Blink for the web page.
     *
     * @param delegate Provides access to the environment in which this runs, e.g. the Activity.
     *     TODO(https://crbug.com/1475820) make this constructor package-private when refactoring.
     */
    @VisibleForTesting
    public DisplayCutoutController(Delegate delegate) {
        mDelegate = delegate;
        mSafeAreaInsetsTracker = new SafeAreaInsetsTrackerImpl();
        maybeAddObservers();
    }

    /**
     * Add observers to {@link InsetObserver} and the browser display cutout mode supplier if we
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

    private void updateInsetObserver(InsetObserver observer) {
        if (mInsetObserver == observer) return;

        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
        }
        mInsetObserver = observer;
        if (mInsetObserver != null) {
            mInsetObserver.addObserver(this);
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

    @Override
    public void destroy() {
        removeObservers();
    }

    /**
     * Set the viewport fit value for the tab.
     * @param value The new viewport fit value.
     */
    public void setViewportFit(@WebContentsObserver.ViewportFitType int value) {
        mSafeAreaInsetsTracker.setIsViewportFitCover(
                value == ViewportFit.COVER || value == ViewportFit.COVER_FORCED_BY_USER_AGENT);

        // TODO(crbug.com/1480477): Investigate whether if() can be turned into assert.
        if (!mDelegate.getWebContents().isFullscreenForCurrentTab()
                && !mDelegate.isInBrowserFullscreen()
                && !mDelegate.isDrawEdgeToEdgeEnabled()) {
            value = ViewportFit.AUTO;
        }

        if (value == mViewportFit) return;

        mViewportFit = value;
        maybeUpdateLayout();
    }

    /**
     * Gets the {@link SafeAreaInsetsTracker} associated with the given Tab.
     *
     * @param tab The {@link Tab} that may have a web page rendered already
     * @return A {@link SafeAreaInsetsTracker} to track changes in insets for viewport-fit=cover.
     */
    public static @Nullable SafeAreaInsetsTracker getSafeAreaInsetsTracker(Tab tab) {
        DisplayCutoutController displayCutoutControllerInstanceForTab = from(tab);
        if (displayCutoutControllerInstanceForTab == null) return null;
        return displayCutoutControllerInstanceForTab.mSafeAreaInsetsTracker;
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

        // Notify Blink of the new insets for css env() variables.
        webContents.setDisplayCutoutSafeArea(area);
    }

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
