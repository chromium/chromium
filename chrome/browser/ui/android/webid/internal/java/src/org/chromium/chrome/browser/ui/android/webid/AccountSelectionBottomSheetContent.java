// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This view renders content that gets displayed inside the bottom sheet. This
 * is a simple container for the view which is the current best practice for
 * bottom sheet content.
 */
public class AccountSelectionBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Supplier<Integer> mScrollOffsetSupplier;
    private @Nullable Supplier<Boolean> mBackPressHandler;

    /**
     * Constructs the AccountSelection bottom sheet view.
     */
    AccountSelectionBottomSheetContent(View contentView, Supplier<Integer> scrollOffsetSupplier) {
        mContentView = contentView;
        mScrollOffsetSupplier = scrollOffsetSupplier;
    }

    public void setBackPressHandler(Supplier<Boolean> backPressHandler) {
        mBackPressHandler = backPressHandler;
    }

    @Override
    public void destroy() {}

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
        return mScrollOffsetSupplier.get();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Return true to ensure no SCRIM is created behind the view
        // automatically.
        return true;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public boolean handleBackPress() {
        return mBackPressHandler != null && mBackPressHandler.get();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.account_selection_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.account_selection_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.account_selection_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.account_selection_sheet_closed;
    }
}
