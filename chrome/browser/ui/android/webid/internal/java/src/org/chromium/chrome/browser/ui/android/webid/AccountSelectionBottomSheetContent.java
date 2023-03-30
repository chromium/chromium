// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This view renders content that gets displayed inside the bottom sheet. This
 * is a simple container for the view which is the current best practice for
 * bottom sheet content.
 */
public class AccountSelectionBottomSheetContent implements BottomSheetContent {
    /**
     * The maximum number of accounts that should be fully visible when the
     * the account picker is displayed.
     */
    private static final float MAX_VISIBLE_ACCOUNTS = 2.5f;
    private final View mContentView;
    private final Supplier<Integer> mScrollOffsetSupplier;
    private @Nullable Runnable mBackPressHandler;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    /**
     * Constructs the AccountSelection bottom sheet view.
     */
    AccountSelectionBottomSheetContent(View contentView, Supplier<Integer> scrollOffsetSupplier) {
        mContentView = contentView;
        mScrollOffsetSupplier = scrollOffsetSupplier;
    }

    /**
     * Updates the sheet content back press handling behavior. This should be invoked during an
     * event that updates the back press handling behavior of the sheet content.
     * @param backPressHandler A runnable that will be invoked by the sheet content to handle a back
     *         press. A null value indicates that back press will not be handled by the content.
     */
    public void setCustomBackPressBehavior(@Nullable Runnable backPressHandler) {
        mBackPressHandler = backPressHandler;
        mBackPressStateChangedSupplier.set(backPressHandler != null);
    }

    public void computeAndUpdateAccountListHeight() {
        // {@link mContentView} is null for some tests.
        if (mContentView == null) return;

        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        // When we're in the multi-account chooser and there are more than {@link
        // MAX_VISIBLE_ACCOUNTS} accounts, resize the list so that only {@link MAX_VISIBLE_ACCOUNTS}
        // accounts and part of the next one are visible.
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        int numAccounts = sheetItemListView.getAdapter().getItemCount();
        if (numAccounts > MAX_VISIBLE_ACCOUNTS) {
            sheetItemListView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            int measuredHeight = sheetItemListView.getMeasuredHeight();
            int containerHeight =
                    Math.round(((float) measuredHeight / numAccounts) * MAX_VISIBLE_ACCOUNTS);
            sheetContainer.getLayoutParams().height = containerHeight;
        } else {
            // Need to set the height here in case it was changed by a previous {@link
            // computeAndUpdateAccountListHeight()} call.
            sheetContainer.getLayoutParams().height = FrameLayout.LayoutParams.WRAP_CONTENT;
        }
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
        return true;
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
        if (mBackPressHandler != null) {
            mBackPressHandler.run();
            return true;
        }
        return false;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        handleBackPress();
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
