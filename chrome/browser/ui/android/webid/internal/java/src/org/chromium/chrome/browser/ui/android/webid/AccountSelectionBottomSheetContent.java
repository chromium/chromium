// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.blink.mojom.RpMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This view renders content that gets displayed inside the bottom sheet. This is a simple container
 * for the view which is the current best practice for bottom sheet content.
 */
public class AccountSelectionBottomSheetContent implements BottomSheetContent {
    /**
     * The maximum number of accounts that should be fully visible when the the account picker is
     * displayed. Active mode UI is generally bigger because it requires user interaction to
     * trigger. Therefore, we are able to show more accounts at once compared to passive mode.
     */
    private static final float MAX_VISIBLE_ACCOUNTS_WIDGET_MODE = 2.5f;

    private static final float MAX_VISIBLE_ACCOUNTS_BUTTON_MODE = 3.5f;

    private final View mContentView;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Integer> mScrollOffsetSupplier;
    private final @RpMode.EnumType int mRpMode;
    private @Nullable Runnable mBackPressHandler;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    /** Constructs the AccountSelection bottom sheet view. */
    AccountSelectionBottomSheetContent(
            View contentView,
            BottomSheetController bottomSheetController,
            Supplier<Integer> scrollOffsetSupplier,
            @RpMode.EnumType int rpMode) {
        mContentView = contentView;
        mBottomSheetController = bottomSheetController;
        mScrollOffsetSupplier = scrollOffsetSupplier;
        mRpMode = rpMode;
    }

    /**
     * Updates the sheet content back press handling behavior. This should be invoked during an
     * event that updates the back press handling behavior of the sheet content.
     *
     * @param backPressHandler A runnable that will be invoked by the sheet content to handle a back
     *     press. A null value indicates that back press will not be handled by the content.
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
        // MAX_VISIBLE_ACCOUNTS_WIDGET_MODE} accounts, resize the list so that only {@link
        // MAX_VISIBLE_ACCOUNTS_WIDGET_MODE}
        // accounts and part of the next one are visible.
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        int numAccounts = sheetItemListView.getAdapter().getItemCount();
        if (numAccounts > MAX_VISIBLE_ACCOUNTS_WIDGET_MODE) {
            sheetItemListView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            int measuredHeight = sheetItemListView.getMeasuredHeight();
            int containerHeight =
                    Math.round(
                            ((float) measuredHeight / numAccounts)
                                    * MAX_VISIBLE_ACCOUNTS_WIDGET_MODE);
            sheetContainer.getLayoutParams().height = containerHeight;
        } else {
            // Need to set the height here in case it was changed by a previous {@link
            // computeAndUpdateAccountListHeight()} call.
            sheetContainer.getLayoutParams().height = FrameLayout.LayoutParams.WRAP_CONTENT;
        }
    }

    /**
     * Returns the height of the full state in active mode.
     *
     * @return the full state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getMaximumActiveModeSheetHeightPx() {
        View accountSelectionSheet = mContentView.findViewById(R.id.account_selection_sheet);
        accountSelectionSheet.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        return accountSelectionSheet.getMeasuredHeight();
    }

    /**
     * Returns the height of the half state in active mode. For up to 3 accounts, it shows all
     * accounts fully. For 4+ accounts, it shows the first 3.5 accounts to encourage scrolling.
     *
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getDesiredActiveModeSheetHeightPx() {
        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        // When we're in the multi-account chooser and there are more than {@link
        // MAX_VISIBLE_ACCOUNTS_BUTTON_MODE} accounts, resize the list so that only {@link
        // MAX_VISIBLE_ACCOUNTS_BUTTON_MODE}
        // accounts and part of the next one are visible.
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        int numAccounts = sheetItemListView.getAdapter().getItemCount();
        if (numAccounts > MAX_VISIBLE_ACCOUNTS_BUTTON_MODE) {
            sheetItemListView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            View accountRow = sheetItemListView.getChildAt(0);
            @Px int measuredHeight = sheetItemListView.getMeasuredHeight();
            @Px
            int desiredHeight =
                    Math.round(accountRow.getMeasuredHeight() * MAX_VISIBLE_ACCOUNTS_BUTTON_MODE);
            return getMaximumActiveModeSheetHeightPx() - measuredHeight + desiredHeight;
        }
        return getMaximumActiveModeSheetHeightPx();
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
        // For passive mode, return true to ensure no scrim is created behind the view.
        if (mRpMode == RpMode.PASSIVE) return true;
        return false;
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
        if (mRpMode == RpMode.PASSIVE) return HeightMode.WRAP_CONTENT;
        // WRAP_CONTENT would be the right fit but this disables the HALF state.
        return Math.min(
                        getMaximumActiveModeSheetHeightPx(),
                        mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        if (mRpMode == RpMode.PASSIVE) {
            computeAndUpdateAccountListHeight();
            return HeightMode.WRAP_CONTENT;
        }
        return Math.min(
                        getDesiredActiveModeSheetHeightPx(),
                        mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
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
