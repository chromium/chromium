// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.function.BooleanSupplier;
import java.util.function.Function;

/**
 * Coordinator for PageInsights bottom sheet module. Provides API, and initializes
 * various components lazily.
 */
public class PageInsightsCoordinator {

    private final Context mContext;

    private final ObservableSupplier<Tab> mTabProvider;
    private final ManagedBottomSheetController mBottomSheetController;
    private final BottomSheetController mBottomUiController;
    private final BrowserControlsStateProvider mControlsStateProvider;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ExpandedSheetHelper mExpandedSheetHelper;

    private final PageInsightsMediator mMediator;

    /** Returns true if page insight is enabled in the feature flag. */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.sCctPageInsightsHub.isEnabled();
    }

    /**
     * Constructor.
     *
     * @param context The associated {@link Context}.
     * @param layoutView the top-level view for the Window
     * @param tabProvider Provider of the current activity tab.
     * @param shareDelegateSupplier Supplier of {@link ShareDelegate}.
     * @param profileSupplier Supplier of {@link Profile}.
     * @param bottomSheetController {@link ManagedBottomSheetController} for page insights.
     * @param bottomUiController {@link BottomSheetController} for other bottom sheet UIs.
     * @param expandedSheetHelper Helps interaction with other UI in expanded mode.
     * @param controlsStateProvider Provides the browser controls' state.
     * @param browserControlsSizer Bottom browser controls resizer.
     * @param backPressManager Back press manager.
     * @param inMotionSupplier Supplier for whether the compositor is in motion.
     * @param isPageInsightsHubEnabled Supplier of the feature flag.
     * @param firstLoadTimeMs Timestamp for the first page load completion.
     */
    public PageInsightsCoordinator(
            Context context,
            View layoutView,
            ObservableSupplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Profile> profileSupplier,
            ManagedBottomSheetController bottomSheetController,
            BottomSheetController bottomUiController,
            ExpandedSheetHelper expandedSheetHelper,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer,
            @Nullable BackPressManager backPressManager,
            @Nullable ObservableSupplier<Boolean> inMotionSupplier,
            BooleanSupplier isPageInsightsEnabledSupplier,
            Function<NavigationHandle, PageInsightsConfig> pageInsightsConfigProvider) {
        mContext = context;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mExpandedSheetHelper = expandedSheetHelper;
        mBottomUiController = bottomUiController;
        mControlsStateProvider = controlsStateProvider;
        mBrowserControlsSizer = browserControlsSizer;
        mMediator =
                new PageInsightsMediator(
                        mContext,
                        layoutView,
                        mTabProvider,
                        shareDelegateSupplier,
                        profileSupplier,
                        mBottomSheetController,
                        mBottomUiController,
                        mExpandedSheetHelper,
                        mControlsStateProvider,
                        mBrowserControlsSizer,
                        backPressManager,
                        inMotionSupplier,
                        isPageInsightsEnabledSupplier,
                        pageInsightsConfigProvider);
    }

    /** Launch PageInsights hub in bottom sheet container and fetch the data to show. */
    public void launch() {
        mMediator.launch();
    }

    /**
     * Initialize bottom sheet view.
     * @param bottomSheetContainer The view containing PageInsights bottom sheet content view.
     */
    public void initView(View bottomSheetContainer) {
        mMediator.initView(bottomSheetContainer);
    }

    /**
     * Notify other bottom UI state is updated. Page insight should be hidden or restored
     * accordingly.
     * @param opened {@code true} if other bottom UI just opened; {@code false} if closed.
     */
    public void onBottomUiStateChanged(boolean opened) {
        mMediator.onBottomUiStateChanged(opened);
    }

    /** Returns the controller for the Page Insights bottom sheet. */
    // TODO(b/307046796): Remove this once we have found better way to integrate with back handling
    // logic.
    public ManagedBottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    /** Destroy PageInsights component. */
    public void destroy() {
        if (mMediator != null) mMediator.destroy();
    }

    float getCornerRadiusForTesting() {
        return mMediator.getCornerRadiusForTesting();
    }

    void onAutoTriggerTimerFinishedForTesting() {
        mMediator.onAutoTriggerTimerFinished();
    }

    void setPageInsightsDataLoaderForTesting(PageInsightsDataLoader pageInsightsDataLoader) {
        mMediator.setPageInsightsDataLoaderForTesting(pageInsightsDataLoader);
    }

    View getContainerForTesting() {
        return mMediator.getContainerForTesting();
    }
}
