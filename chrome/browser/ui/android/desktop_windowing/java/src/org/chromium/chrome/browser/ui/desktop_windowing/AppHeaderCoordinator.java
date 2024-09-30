// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static android.view.WindowInsetsController.APPEARANCE_LIGHT_CAPTION_BARS;
import static android.view.WindowInsetsController.APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsetsController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowHeuristicResult;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetObserver;
import org.chromium.ui.InsetsRectProvider;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

/**
 * Class coordinating the business logic to draw into app header in desktop windowing mode, ranging
 * from listening the window insets updates, and pushing updates to the tab strip.
 */
@RequiresApi(VERSION_CODES.R)
public class AppHeaderCoordinator
        implements DesktopWindowStateProvider,
                TopResumedActivityChangedObserver,
                SaveInstanceStateObserver {
    @VisibleForTesting
    public static final String INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW =
            "is_app_in_unfocused_desktop_window";

    private static final String TAG = "AppHeader";

    private static @Nullable InsetsRectProvider sInsetsRectProviderForTesting;

    private Activity mActivity;
    private final View mRootView;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final InsetObserver mInsetObserver;
    private final InsetsRectProvider mCaptionBarRectProvider;
    private final WindowInsetObserver mWindowInsetObserver;
    private final WindowInsetsController mInsetsController;
    private final ObserverList<AppHeaderObserver> mObservers = new ObserverList<>();
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // Internal states
    private boolean mIsInDesktopWindow;
    private boolean mIsEdgeToEdgeActive;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private @Nullable AppHeaderState mAppHeaderState;
    private boolean mIsInUnfocusedDesktopWindow;
    private @DesktopWindowHeuristicResult int mHeuristicResult =
            DesktopWindowHeuristicResult.UNKNOWN;
    private int mKeyboardInset;
    private int mSystemBarBottomInset;

    /**
     * Instantiate the coordinator to handle drawing the tab strip into the captionBar area.
     *
     * @param activity The activity associated with the window containing the app header.
     * @param rootView The root view within the activity.
     * @param browserControlsVisibilityDelegate Delegate interface allowing control of the browser
     *     controls visibility.
     * @param insetObserver {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} to dispatch {@link
     *     TopResumedActivityChangedObserver#onTopResumedActivityChanged(boolean)} and {@link
     *     SaveInstanceStateObserver#onSaveInstanceState(Bundle)} events observed by this class.
     * @param savedInstanceState The saved instance state {@link Bundle} holding UI state
     *     information for restoration on startup.
     */
    public AppHeaderCoordinator(
            Activity activity,
            View rootView,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            @NonNull InsetObserver insetObserver,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Bundle savedInstanceState) {
        mActivity = activity;
        mRootView = rootView;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mInsetObserver = insetObserver;
        mWindowInsetObserver =
                new WindowInsetObserver() {
                    @Override
                    public void onInsetChanged(int left, int top, int right, int bottom) {
                        mSystemBarBottomInset = bottom;
                        maybeUpdateRootViewBottomPadding();
                    }

                    @Override
                    public void onKeyboardInsetChanged(int inset) {
                        mKeyboardInset = inset;
                        maybeUpdateRootViewBottomPadding();
                    }
                };
        mInsetObserver.addObserver(mWindowInsetObserver);
        mInsetsController = mRootView.getWindowInsetsController();
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        // Whether the app started in an unfocused desktop window, so that relevant UI state can be
        // restored.
        mIsInUnfocusedDesktopWindow =
                savedInstanceState != null
                        && savedInstanceState.getBoolean(
                                INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, false);

        // Initialize mInsetsRectProvider and setup observers.
        mCaptionBarRectProvider =
                sInsetsRectProviderForTesting != null
                        ? sInsetsRectProviderForTesting
                        : new InsetsRectProvider(
                                insetObserver,
                                WindowInsetsCompat.Type.captionBar(),
                                insetObserver.getLastRawWindowInsets());
        InsetsRectProvider.Observer insetsRectUpdateRunnable = this::onInsetsRectsUpdated;
        mCaptionBarRectProvider.addObserver(insetsRectUpdateRunnable);

        // Populate the initial value if the rect provider is ready.
        if (!mCaptionBarRectProvider.getWidestUnoccludedRect().isEmpty()) {
            insetsRectUpdateRunnable.onBoundingRectsUpdated(
                    mCaptionBarRectProvider.getWidestUnoccludedRect());
        }
    }

    /** Destroy the instances and remove all the dependencies. */
    public void destroy() {
        mActivity = null;
        mCaptionBarRectProvider.destroy();
        mInsetObserver.removeObserver(mWindowInsetObserver);
        mObservers.clear();
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public AppHeaderState getAppHeaderState() {
        return mAppHeaderState;
    }

    // TODO(crbug.com/337086192): Read from mAppHeaderState.
    @Override
    public boolean isInDesktopWindow() {
        return mIsInDesktopWindow;
    }

    @Override
    public boolean isInUnfocusedDesktopWindow() {
        return mIsInUnfocusedDesktopWindow;
    }

    @Override
    public boolean addObserver(AppHeaderObserver observer) {
        return mObservers.addObserver(observer);
    }

    @Override
    public boolean removeObserver(AppHeaderObserver observer) {
        return mObservers.removeObserver(observer);
    }

    @Override
    public void updateForegroundColor(int backgroundColor) {
        updateIconColorForCaptionBars(backgroundColor);
    }

    // TopResumedActivityChangedObserver implementation.
    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        mIsInUnfocusedDesktopWindow = !isTopResumedActivity && mIsInDesktopWindow;
    }

    // SaveInstanceStateObserver implementation.
    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, mIsInUnfocusedDesktopWindow);
    }

    private void onInsetsRectsUpdated(@NonNull Rect widestUnoccludedRect) {
        mHeuristicResult =
                checkIsInDesktopWindow(
                        mActivity, mInsetObserver, mCaptionBarRectProvider, mHeuristicResult);
        var isInDesktopWindow = mHeuristicResult == DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW;
        // Use an empty |widestUnoccludedRect| instead of the cached Rect while creating the
        // AppHeaderState while not in or while exiting desktop windowing mode, so that it always
        // holds a valid state for observers to use.
        var appHeaderState =
                new AppHeaderState(
                        mCaptionBarRectProvider.getWindowRect(),
                        isInDesktopWindow
                                ? mCaptionBarRectProvider.getWidestUnoccludedRect()
                                : new Rect(),
                        isInDesktopWindow);
        if (appHeaderState.equals(mAppHeaderState)) return;

        boolean desktopWindowingModeChanged = mIsInDesktopWindow != isInDesktopWindow;
        mIsInDesktopWindow = isInDesktopWindow;
        // Note that this E2E state may be inconsistent in reality, especially because there may be
        // other callers of #setDecorFitsSystemWindows(). It is assumed here that E2E is always
        // consistent with desktop windowing mode.
        mIsEdgeToEdgeActive = mIsInDesktopWindow;
        mAppHeaderState = appHeaderState;

        // Update the root-level content view's padding to account for bottom inset changes.
        maybeUpdateRootViewBottomPadding();

        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
        }

        // If whether we are in DW mode does not change, we can end this method now.
        if (!desktopWindowingModeChanged) return;
        for (var observer : mObservers) {
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }

        // 1. Enter E2E if we are in desktop windowing mode.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), !mIsEdgeToEdgeActive);

        // 2. Set the captionBar background appropriately to draw into the region.
        updateCaptionBarBackground(mIsInDesktopWindow);

        // 3. Lock the browser controls when we are in DW mode.
        if (mIsInDesktopWindow) {
            mBrowserControlsToken =
                    mBrowserControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mBrowserControlsToken);
        } else {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(mBrowserControlsToken);
        }
    }

    /**
     * Check if the desktop windowing mode is enabled by checking all the criteria:
     *
     * <ol type=1>
     *   <li>Caption bar has insets.top > 0;
     *   <li>There's no bottom insets from the navigation bar;
     *   <li>Caption bar has 2 bounding rects;
     *   <li>Widest unoccluded rect in captionBar insets is connected to the bottom;
     * </ol>
     *
     * This method is marked as static, in order to ensure it does not change / read any state from
     * an AppHeaderCoordinator instance, especially the cached {@link AppHeaderState}.
     */
    private static @DesktopWindowHeuristicResult int checkIsInDesktopWindow(
            Activity activity,
            InsetObserver insetObserver,
            InsetsRectProvider insetsRectProvider,
            @DesktopWindowHeuristicResult int currentResult) {
        @DesktopWindowHeuristicResult int newResult;

        assert insetObserver.getLastRawWindowInsets() != null
                : "Attempt to read the insets too early.";
        var navBarInsets =
                insetObserver
                        .getLastRawWindowInsets()
                        .getInsets(WindowInsetsCompat.Type.navigationBars());

        int numOfBoundingRects = insetsRectProvider.getBoundingRects().size();
        Insets captionBarInset = insetsRectProvider.getCachedInset();

        if (!activity.isInMultiWindowMode()) {
            newResult = DesktopWindowHeuristicResult.NOT_IN_MULTIWINDOW_MODE;
        } else if (navBarInsets.bottom > 0) {
            // Disable DW mode if there is a navigation bar (though it may or may not be visible /
            // dismissed).
            newResult = DesktopWindowHeuristicResult.NAV_BAR_BOTTOM_INSETS_PRESENT;
        } else if (numOfBoundingRects != 2) {
            Log.w(TAG, "Unexpected number of bounding rects is observed! " + numOfBoundingRects);
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECTS_UNEXPECTED_NUMBER;
        } else if (captionBarInset.top == 0) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT;
        } else if (insetsRectProvider.getWidestUnoccludedRect().bottom != captionBarInset.top) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT;
        } else {
            newResult = DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW;
        }
        if (newResult != currentResult) {
            Log.i(TAG, "Recording desktop windowing heuristic result: " + newResult);
            // Only record histogram when heuristics result has changed.
            AppHeaderUtils.recordDesktopWindowHeuristicResult(newResult);
        }
        return newResult;
    }

    private void updateCaptionBarBackground(boolean isTransparent) {
        int captionBarAppearance =
                isTransparent ? APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND : 0;
        int currentCaptionBarAppearance =
                mInsetsController.getSystemBarsAppearance()
                        & APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND;
        // This is a workaround to prevent #setSystemBarsAppearance to trigger infinite inset
        // updates.
        if (currentCaptionBarAppearance != captionBarAppearance) {
            mInsetsController.setSystemBarsAppearance(
                    captionBarAppearance, APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND);
        }
    }

    private void updateIconColorForCaptionBars(int color) {
        boolean useLightIcon = ColorUtils.shouldUseLightForegroundOnBackground(color);
        // APPEARANCE_LIGHT_CAPTION_BARS needs to be set when caption bar is with light background.
        int captionBarAppearance = useLightIcon ? 0 : APPEARANCE_LIGHT_CAPTION_BARS;
        mInsetsController.setSystemBarsAppearance(
                captionBarAppearance, APPEARANCE_LIGHT_CAPTION_BARS);
    }

    // Desktop windows use E2E display and the root-level content view's bottom padding needs to be
    // updated to "resize" the content view and restrict showing bottom Chrome UI within these
    // bounds.
    // TODO (crbug/325506516): Remove this logic when E2E implementation handles this.
    private void maybeUpdateRootViewBottomPadding() {
        int rootViewBottomPadding = mRootView.getPaddingBottom();
        // Pad the root view with bottom window insets only if E2E is active.
        int bottomInset =
                !mIsEdgeToEdgeActive ? 0 : Math.max(mKeyboardInset, mSystemBarBottomInset);

        // If the root view is padded as needed already, return early.
        if (rootViewBottomPadding == bottomInset) return;

        mRootView.setPadding(
                mRootView.getPaddingLeft(),
                mRootView.getPaddingTop(),
                mRootView.getPaddingRight(),
                bottomInset);
    }

    /** Set states for testing. */
    public void setStateForTesting(boolean isInDesktopWindow, AppHeaderState appHeaderState) {
        mIsInDesktopWindow = isInDesktopWindow;
        mIsEdgeToEdgeActive = isInDesktopWindow;
        mAppHeaderState = appHeaderState;

        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }
    }

    public static void setInsetsRectProviderForTesting(InsetsRectProvider providerForTesting) {
        sInsetsRectProviderForTesting = providerForTesting;
        ResettersForTesting.register(() -> sInsetsRectProviderForTesting = null);
    }

    WindowInsetObserver getWindowInsetObserverForTesting() {
        return mWindowInsetObserver;
    }
}
