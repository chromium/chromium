// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static android.view.WindowInsetsController.APPEARANCE_LIGHT_CAPTION_BARS;
import static android.view.WindowInsetsController.APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import static java.lang.Boolean.FALSE;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsetsController;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowHeuristicResult;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.WindowingMode;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.CaptionBarInsetsRectProvider;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.InsetsRectProvider;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

/**
 * Class coordinating the business logic to draw into app header in desktop windowing mode, ranging
 * from listening the window insets updates, and pushing updates to the tab strip.
 */
@RequiresApi(VERSION_CODES.R)
@NullMarked
public class AppHeaderCoordinator
        implements DesktopWindowStateManager,
                TopResumedActivityChangedObserver,
                SaveInstanceStateObserver,
                WindowInsetsConsumer {
    @VisibleForTesting
    public static final String INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW =
            "is_app_in_unfocused_desktop_window";

    private static final String TAG = "AppHeader";

    private static @Nullable CaptionBarInsetsRectProvider sInsetsRectProviderForTesting;

    private @Nullable Activity mActivity;
    private final View mRootView;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final InsetObserver mInsetObserver;
    private final CaptionBarInsetsRectProvider mCaptionBarRectProvider;
    private final WindowInsetsController mInsetsController;
    private final ObserverList<AppHeaderObserver> mObservers = new ObserverList<>();
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // Internal states
    private boolean mIsInDesktopWindow;
    private final EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    private int mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private @Nullable AppHeaderState mAppHeaderState;
    private boolean mIsInUnfocusedDesktopWindow;
    private @DesktopWindowHeuristicResult int mHeuristicResult =
            DesktopWindowHeuristicResult.UNKNOWN;
    private @WindowingMode int mWindowingMode = WindowingMode.UNKNOWN;
    private int mKeyboardInset;
    private int mNavBarInset;

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
     * @param edgeToEdgeStateProvider The {@link EdgeToEdgeStateProvider} to determine the
     *     edge-to-edge state.
     */
    public AppHeaderCoordinator(
            Activity activity,
            View rootView,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            InsetObserver insetObserver,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Bundle savedInstanceState,
            EdgeToEdgeStateProvider edgeToEdgeStateProvider) {
        mActivity = activity;
        mEdgeToEdgeStateProvider = edgeToEdgeStateProvider;
        mRootView = rootView;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mInsetObserver = insetObserver;
        mInsetObserver.addInsetsConsumer(this, InsetConsumerSource.APP_HEADER_COORDINATOR_BOTTOM);
        mInsetsController = assertNonNull(mRootView.getWindowInsetsController());
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
                        : new CaptionBarInsetsRectProvider(
                                insetObserver,
                                insetObserver.getLastRawWindowInsets(),
                                InsetConsumerSource.APP_HEADER_COORDINATOR_CAPTION);

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
        mInsetObserver.removeInsetsConsumer(this);
        mObservers.clear();
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public @Nullable AppHeaderState getAppHeaderState() {
        return mAppHeaderState;
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

    private void onInsetsRectsUpdated(Rect widestUnoccludedRect) {
        // mActivity is only set to null in destroy().
        boolean isOnExternalDisplay =
                !DisplayUtil.isContextInDefaultDisplay(assumeNonNull(mActivity));
        mHeuristicResult =
                checkIsInDesktopWindow(
                        mCaptionBarRectProvider, mHeuristicResult, isOnExternalDisplay);
        var isInDesktopWindow = mHeuristicResult == DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW;

        // Avoid determining the mode when there are no window insets, which may be the case in the
        // middle of a windowing mode change. Presence of insets indicates that the window is in a
        // stable state.
        assert mInsetObserver.getLastRawWindowInsets() != null
                : "Attempt to read the insets too early.";
        if (mInsetObserver.getLastRawWindowInsets().hasInsets()) {
            mWindowingMode =
                    AppHeaderUtils.getWindowingMode(mActivity, isInDesktopWindow, mWindowingMode);
        }

        var appHeaderState =
                new AppHeaderState(
                        mCaptionBarRectProvider.getWindowRect(),
                        widestUnoccludedRect,
                        isInDesktopWindow);
        if (appHeaderState.equals(mAppHeaderState)) return;

        boolean desktopWindowingModeChanged = mIsInDesktopWindow != isInDesktopWindow;
        mIsInDesktopWindow = isInDesktopWindow;
        mAppHeaderState = appHeaderState;

        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
        }

        // If whether we are in DW mode does not change, we can end this method now.
        if (!desktopWindowingModeChanged) return;

        for (var observer : mObservers) {
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }

        // 1. Enter E2E if we are in desktop windowing mode.
        setEdgeToEdgeState(mIsInDesktopWindow);

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
     *   <li>Widest unoccluded rect in caption bar has space available to draw the tab strip;
     *   <li>Widest unoccluded rect in captionBar insets is connected to the bottom;
     *   <li>Header customization is not disallowed;
     * </ol>
     *
     * This method is marked as static, in order to ensure it does not change / read any state from
     * an AppHeaderCoordinator instance, especially the cached {@link AppHeaderState}.
     */
    private static @DesktopWindowHeuristicResult int checkIsInDesktopWindow(
            InsetsRectProvider insetsRectProvider,
            @DesktopWindowHeuristicResult int currentResult,
            boolean isOnExternalDisplay) {
        @DesktopWindowHeuristicResult int newResult;

        Insets captionBarInset = insetsRectProvider.getCachedInset();
        boolean allowHeaderCustomization =
                AppHeaderUtils.shouldAllowHeaderCustomizationOnNonDefaultDisplay()
                        || !isOnExternalDisplay;

        if (insetsRectProvider.getWidestUnoccludedRect().isEmpty()) {
            newResult = DesktopWindowHeuristicResult.WIDEST_UNOCCLUDED_RECT_EMPTY;
        } else if (captionBarInset.top == 0) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT;
        } else if (insetsRectProvider.getWidestUnoccludedRect().bottom != captionBarInset.top) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT;
        } else if (!allowHeaderCustomization) {
            newResult = DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY;
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

    /**
     * Update the root-level content view's bottom padding to "resize" the content view and restrict
     * showing bottom Chrome UI within these bounds in a desktop window, where E2E is active.
     *
     * @return {@code true} if a non-zero bottom padding is applied to the content view, {@code
     *     false} otherwise.
     */
    // TODO (crbug/325506516): Remove this logic when E2E implementation handles this.
    private boolean maybeUpdateRootViewBottomPadding() {
        int rootViewBottomPadding = mRootView.getPaddingBottom();
        // Pad the root view with IME bottom insets only if E2E is active.
        int bottomInset =
                FALSE.equals(mEdgeToEdgeStateProvider.get())
                        ? 0
                        : Math.max(mKeyboardInset, mNavBarInset);

        // If the root view is padded as needed already, return early.
        if (rootViewBottomPadding == bottomInset) return bottomInset != 0;

        mRootView.setPadding(
                mRootView.getPaddingLeft(),
                mRootView.getPaddingTop(),
                mRootView.getPaddingRight(),
                bottomInset);
        return bottomInset != 0;
    }

    /** Set states for testing. */
    public void setStateForTesting(boolean isInDesktopWindow, AppHeaderState appHeaderState) {
        mIsInDesktopWindow = isInDesktopWindow;
        setEdgeToEdgeState(mIsInDesktopWindow);
        mAppHeaderState = appHeaderState;

        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }
    }

    public static void setInsetsRectProviderForTesting(
            CaptionBarInsetsRectProvider providerForTesting) {
        sInsetsRectProviderForTesting = providerForTesting;
        ResettersForTesting.register(() -> sInsetsRectProviderForTesting = null);
    }

    private void setEdgeToEdgeState(boolean active) {
        if (active) {
            mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        } else {
            mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(mEdgeToEdgeToken);
            mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
        }
    }

    // WindowInsetsConsumer implementation.

    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        mKeyboardInset = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;
        mNavBarInset =
                windowInsetsCompat.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom;
        boolean resizedRootView = maybeUpdateRootViewBottomPadding();
        if (!resizedRootView) return windowInsetsCompat;

        // Consume IME insets if the root view has been adjusted.
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE)
                .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                .build();
    }
}
