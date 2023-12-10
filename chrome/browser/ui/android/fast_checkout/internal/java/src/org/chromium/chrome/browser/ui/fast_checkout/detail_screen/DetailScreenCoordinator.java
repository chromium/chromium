// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.HOME_SCREEN;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.FULL;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HALF;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HIDDEN;

import android.content.Context;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.bottom_sheet_utils.DetailScreenScrollListener;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the detail screens (Autofill profile selection, credit card selection)
 * of the Fast Checkout bottom sheet.
 */
public class DetailScreenCoordinator {
    private final PropertyModel mModel;
    private final RecyclerView mRecyclerView;
    private final BottomSheetController mBottomSheetController;
    private final DetailScreenScrollListener mScrollListener;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int state, int reason) {
                    if (state == HIDDEN) {
                        mBottomSheetController.removeObserver(mBottomSheetObserver);
                        return;
                    } else if (state == FULL) {
                        mRecyclerView.suppressLayout(/* suppress= */ false);
                    } else if (state == HALF && mScrollListener.isScrolledToTop()) {
                        mRecyclerView.suppressLayout(/* suppress= */ true);
                    }

                    // The details screen's accessibility overlay is supposed to be
                    // (accessibility-)focused when leaving the home screen. The {@link
                    // BottomSheet} programmatically requests both focuses so they need to be
                    // taken back. Otherwise the bottom sheet announcement would be made instead
                    // of the detail screen's one; or Tab key navigation focus order
                    // would be not as expected. This event is emitted after the bottom sheet's
                    // focus-taking actions.
                    if (mModel.get(CURRENT_SCREEN) != HOME_SCREEN) {
                        View toolbarA11yOverlay =
                                mBottomSheetController
                                        .getCurrentSheetContent()
                                        .getContentView()
                                        .findViewById(R.id.fast_checkout_toolbar_a11y_overlay_view);
                        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                            // Request "accessibility-focus" for TalkBack.
                            toolbarA11yOverlay.sendAccessibilityEvent(
                                    AccessibilityEvent.TYPE_VIEW_FOCUSED);
                        }
                        if (UiUtils.isHardwareKeyboardAttached()) {
                            // Request focus for keyboard navigation.
                            toolbarA11yOverlay.requestFocus();
                        }
                    }
                }
            };

    /**
     * Sets up the view of the detail screen, puts it into a {@link
     * DetailScreenViewBinder.ViewHolder} and connects it to the PropertyModel by setting up a model
     * change processor.
     */
    public DetailScreenCoordinator(
            Context context,
            View view,
            PropertyModel model,
            BottomSheetController bottomSheetController) {
        mModel = model;
        mBottomSheetController = bottomSheetController;
        mScrollListener = new DetailScreenScrollListener(bottomSheetController);

        mRecyclerView = view.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DetailItemDecoration(
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.fast_checkout_detail_sheet_spacing_vertical)));
        mRecyclerView.addOnScrollListener(mScrollListener);
        bottomSheetController.addObserver(mBottomSheetObserver);

        DetailScreenViewBinder.ViewHolder viewHolder =
                new DetailScreenViewBinder.ViewHolder(context, view, mScrollListener);

        PropertyModelChangeProcessor.create(model, viewHolder, DetailScreenViewBinder::bind);
    }
}
