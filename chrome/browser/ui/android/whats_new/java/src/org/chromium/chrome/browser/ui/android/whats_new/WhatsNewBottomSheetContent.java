// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import android.content.Context;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.android.whats_new.WhatsNewProperties.ViewState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Bottom sheet content of the What's New Page. */
@NullMarked
public class WhatsNewBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ViewFlipper mViewFlipperView;

    private final BottomSheetController mBottomSheetController;

    // Helps keep track of whether the Back button was pressed.
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    // The handler to notify when the (Android) Back button is pressed.
    private final Runnable mOsBackButtonClicked;

    WhatsNewBottomSheetContent(
            View contentView,
            BottomSheetController bottomSheetController,
            Runnable onOsBackButtonClicked) {
        mContentView = contentView;
        mViewFlipperView = mContentView.findViewById(R.id.whats_new_page_view_flipper);
        mBottomSheetController = bottomSheetController;

        mOsBackButtonClicked = onOsBackButtonClicked;
        mBackPressStateChangedSupplier.set(true);
    }

    void setViewState(@ViewState int viewState) {
        switch (viewState) {
            case ViewState.HIDDEN:
                hideBottomSheet();
                break;
            case ViewState.OVERVIEW:
                setShowingOverviewView();
                break;
            case ViewState.DETAIL:
                setShowingFeatureDetailView();
                break;
        }
    }

    private void setShowingOverviewView() {
        mViewFlipperView.setDisplayedChild(0);
        mBottomSheetController.requestShowContent(this, /* animate= */ true);
    }

    private void setShowingFeatureDetailView() {
        mViewFlipperView.setDisplayedChild(1);
    }

    private void hideBottomSheet() {
        mBottomSheetController.hideContent(this, /* animate= */ true);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public float getHalfHeightRatio() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        mOsBackButtonClicked.run();
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.whats_new_page_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.whats_new_page_title;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.whats_new_page_title;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.whats_new_page_title;
    }
}
