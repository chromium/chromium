// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.FULL;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HALF;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HIDDEN;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the detail screens (Autofill profile selection, credit card selection)
 * of the Fast Checkout bottom sheet.
 */
public class DetailScreenCoordinator {
    private final RecyclerView mRecyclerView;
    private final BottomSheetController mBottomSheetController;
    private final DetailScreenScrollListener mScrollListener;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int state, int reason) {
            if (state == HIDDEN) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
            } else if (state == FULL) {
                mRecyclerView.suppressLayout(/*suppress=*/false);
            } else if (state == HALF && mScrollListener.isScrolledToTop()) {
                mRecyclerView.suppressLayout(/*suppress=*/true);
            }
        }
    };

    /**
     * Sets up the view of the detail screen, puts it into a {@link
     * DetailScreenViewBinder.ViewHolder} and connects it to the PropertyModel by setting up a model
     * change processor.
     */
    public DetailScreenCoordinator(Context context, View view, PropertyModel model,
            BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mScrollListener = new DetailScreenScrollListener(bottomSheetController);
        Toolbar toolbar = (Toolbar) view.findViewById(R.id.action_bar);
        assert toolbar != null;
        toolbar.inflateMenu(R.menu.fast_checkout_toolbar_menu);
        Drawable tintedBackIcon = TintedDrawable.constructTintedDrawable(toolbar.getContext(),
                R.drawable.ic_arrow_back_white_24dp, R.color.default_icon_color_tint_list);
        toolbar.setNavigationIcon(tintedBackIcon);
        toolbar.setNavigationContentDescription(
                R.string.fast_checkout_back_to_home_screen_icon_description);

        mRecyclerView = view.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DetailItemDecoration(context.getResources().getDimensionPixelSize(
                        R.dimen.fast_checkout_detail_sheet_spacing_vertical)));
        mRecyclerView.addOnScrollListener(mScrollListener);
        bottomSheetController.addObserver(mBottomSheetObserver);

        DetailScreenViewBinder.ViewHolder viewHolder =
                new DetailScreenViewBinder.ViewHolder(context, view, mScrollListener);

        PropertyModelChangeProcessor.create(model, viewHolder, DetailScreenViewBinder::bind);
    }
}
