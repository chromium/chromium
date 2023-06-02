// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

/**
 * Coordinator for PageInsights bottom sheet module. Provides API, and initializes
 * various components lazily.
 */
public class PageInsightsCoordinator {
    private final Context mContext;

    private final ObservableSupplier<Tab> mTabProvider;
    private final ManagedBottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mControlsStateProvider;
    private final BrowserControlsSizer mBrowserControlsSizer;

    private PageInsightsMediator mMediator;
    private PageInsightsSheetContent mSheetContent;

    /**
     * Constructor.
     * @param context The associated {@link Context}.
     * @param tabProvider Provider of the current activity tab.
     * @param bottomSheetController {@link ManagedBottomSheetController} for page insights hub.
     */
    public PageInsightsCoordinator(Context context, ObservableSupplier<Tab> tabProvider,
            ManagedBottomSheetController bottomSheetController,
            BrowserControlsStateProvider controlsStateProvider,
            BrowserControlsSizer browserControlsSizer) {
        mContext = context;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mControlsStateProvider = controlsStateProvider;
        mBrowserControlsSizer = browserControlsSizer;
    }

    /**
     * Launch PageInsights hub in bottom sheet container and fetch the data to show.
     */
    public void launch() {
        if (mSheetContent == null) {
            mSheetContent = new PageInsightsSheetContent(mContext);
        }
        if (mMediator == null) {
            mMediator = new PageInsightsMediator(mSheetContent, mBottomSheetController,
                    mTabProvider, mControlsStateProvider, mBrowserControlsSizer);
        }
        mMediator.requestShowContent();
    }

    /**
     * Initialize bottom sheet view.
     * @param bottomSheetContainer The view containing PageInsights bottom sheet content view.
     */
    public void initView(View bottomSheetContainer) {
        mMediator.initView(bottomSheetContainer);
    }

    @VisibleForTesting
    float getCornerRadiusForTesting() {
        return mMediator.getCornerRadiusForTesting();
    }
}
