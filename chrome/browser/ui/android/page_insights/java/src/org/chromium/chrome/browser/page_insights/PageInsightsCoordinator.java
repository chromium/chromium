// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

import java.util.function.BooleanSupplier;

/**
 * Coordinator for PageInsights bottom sheet module. Provides API, and initializes
 * various components lazily.
 */
public class PageInsightsCoordinator {
    private static MutableFlagWithSafeDefault sPageInsightsHub =
            new MutableFlagWithSafeDefault(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, false);

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
        return sPageInsightsHub.isEnabled();
    }

    /**
     * Constructor.
     * @param context The associated {@link Context}.
     * @param tabProvider Provider of the current activity tab.
     * @param shareDelegateSupplier Supplier of {@link ShareDelegate}.
     * @param bottomSheetController {@link ManagedBottomSheetController} for page insights.
     * @param bottomUiController {@link BottomSheetController} for other bottom sheet UIs.
     * @param expandedSheetHelper Helps interaction with other UI in expanded mode.
     * @param controlsStateProvider Provides the browser controls' state.
     * @param browserControlsSizer Bottom browser controls resizer.
     * @param isPageInsightsHubEnabled Supplier of the feature flag.
     */
    public PageInsightsCoordinator(Context context, ObservableSupplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            ManagedBottomSheetController bottomSheetController,
            BottomSheetController bottomUiController, ExpandedSheetHelper expandedSheetHelper,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer, BooleanSupplier isPageInsightsHubEnabled) {
        mContext = context;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mExpandedSheetHelper = expandedSheetHelper;
        mBottomUiController = bottomUiController;
        mControlsStateProvider = controlsStateProvider;
        mBrowserControlsSizer = browserControlsSizer;
        mMediator = new PageInsightsMediator(mContext, mTabProvider, shareDelegateSupplier,
                mBottomSheetController, mBottomUiController, mExpandedSheetHelper,
                mControlsStateProvider, mBrowserControlsSizer, isPageInsightsHubEnabled);
    }

    /**
     * Launch PageInsights hub in bottom sheet container and fetch the data to show.
     */
    public void launch() {
        mMediator.openInExpandedState();
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

    /** Destroy PageInsights component. */
    public void destroy() {
        if (mMediator != null) mMediator.destroy();
    }

    float getCornerRadiusForTesting() {
        return mMediator.getCornerRadiusForTesting();
    }

    void setAutoTriggerReadyForTesting() {
        mMediator.setAutoTriggerReadyForTesting();
    }

    void setPageInsightsDataLoaderForTesting(PageInsightsDataLoader pageInsightsDataLoader) {
        mMediator.setPageInsightsDataLoaderForTesting(pageInsightsDataLoader);
    }

    View getContainerForTesting() {
        return mMediator.getContainerForTesting();
    }
}
