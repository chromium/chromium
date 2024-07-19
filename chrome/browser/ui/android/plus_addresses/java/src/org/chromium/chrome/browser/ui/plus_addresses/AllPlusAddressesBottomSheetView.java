// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Implements the bottom sheet content for the all plus addresses bottom sheet. */
class AllPlusAddressesBottomSheetView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private final LinearLayout mContentView;

    public AllPlusAddressesBottomSheetView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.all_plus_addresses_bottom_sheet, null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
    }

    void setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.requestShowContent(this, true);
        } else {
            mBottomSheetController.hideContent(this, true);
        }
    }

    void setTitle(String title) {
        ((TextView) mContentView.findViewById(R.id.sheet_title)).setText(title);
    }

    void setWarning(String warning) {
        ((TextView) mContentView.findViewById(R.id.sheet_warning)).setText(warning);
    }

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
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
    public float getHalfHeightRatio() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO: crbug.com/327838324 - Implement accessibility strings.
        return R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO: crbug.com/327838324 - Implement accessibility strings.
        return R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO: crbug.com/327838324 - Implement accessibility strings.
        return R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO: crbug.com/327838324 - Implement accessibility strings.
        return R.string.ok;
    }
}
