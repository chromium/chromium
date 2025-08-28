// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/**
 * This view renders content that gets displayed inside the bottom sheet. This is a simple container
 * for the view which is the current best practice for bottom sheet content.
 */
public class AccountSelectionBottomSheetContent implements BottomSheetContent {
    /**
     * The maximum number of accounts that should be fully visible when the the account picker is
     * displayed. Active mode UI is generally bigger because it requires user interaction to
     * trigger. Therefore, we are able to show more accounts at once compared to passive mode. And
     * multi IDP UI accounts take more space, so we show even less accounts in that case.
     */
    private static final float MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_MULTI_IDP = 1.4f;

    private static final float MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_SINGLE_IDP = 2.5f;

    private static final float MAX_VISIBLE_ACCOUNTS_BUTTON_MODE = 3.5f;

    private static final int MIN_NUM_ACCOUNTS_FOR_SCROLL = 3;

    private final View mContentView;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Integer> mScrollOffsetSupplier;
    private final @RpMode.EnumType int mRpMode;
    private @Nullable Runnable mBackPressHandler;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    private boolean mIsMultipleIdps;
    // Used to disable the half state in passive mode to enable proper a11y traversal.
    private boolean mIsPassiveModeHalfHeightEnabled = true;
    private boolean mCustomAccessibilityDelegateSet;

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

    public void setIsMultipleIdps(boolean isMultipleIdps) {
        mIsMultipleIdps = isMultipleIdps;
    }

    public void setIsPassiveModeHalfHeightEnabled(boolean isPassiveModeHalfHeightEnabled) {
        mIsPassiveModeHalfHeightEnabled = isPassiveModeHalfHeightEnabled;
    }

    public void computeAndUpdateAccountListHeightForPassiveSingleIdp() {
        // {@link mContentView} is null for some tests.
        if (mContentView == null) return;

        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        // When we're in the multi-account chooser and there are more than {@link
        // MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_SINGLE_IDP} accounts, resize the list so that only
        // {@link
        // MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_SINGLE_IDP}
        // accounts and part of the next one are visible.
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        int numAccounts = sheetItemListView.getAdapter().getItemCount();

        // When the number of rows is just over the limit and the last one is the user a different
        // account button, we increase the max to avoid cutting off the use a different account
        // button since its size is a bit different so it looks odd.
        float maxRowSize = MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_SINGLE_IDP;
        if (sheetItemListView.findViewById(R.id.add_account) != null
                && numAccounts == (int) (maxRowSize + 1)) {
            ++maxRowSize;
        }
        if (numAccounts > maxRowSize) {
            sheetItemListView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            int accountHeight = sheetItemListView.getChildAt(0).getMeasuredHeight();
            int containerHeight = Math.round(accountHeight * maxRowSize);
            sheetContainer.getLayoutParams().height = containerHeight;
        } else {
            // Need to set the height here in case it was changed by a previous {@link
            // computeAndUpdateAccountListHeightForPassiveSingleIdp()} call.
            sheetContainer.getLayoutParams().height = FrameLayout.LayoutParams.WRAP_CONTENT;
        }
    }

    /**
     * Returns the maximum height of the sheet.
     *
     * @return the full state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getMaximumSheetHeightPx() {
        int width = mBottomSheetController.getMaxSheetWidth();
        View accountSelectionSheet = mContentView.findViewById(R.id.account_selection_sheet);
        accountSelectionSheet.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
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
        return getDesiredSheetHeight(MAX_VISIBLE_ACCOUNTS_BUTTON_MODE, getMaximumSheetHeightPx());
    }

    /**
     * Returns the height of the half state. Returns maxHeight if the number of rows is less than
     * MIN_NUM_ACCOUNTS_FOR_SCROLL.
     *
     * @param maxVisibleRows The maximum number of accounts that are shown.
     * @param maxHeightPx The maximum height of the sheet.
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getDesiredSheetHeight(float maxVisibleRows, @Px int maxHeightPx) {
        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        // When we're in the multi-account chooser and there are more than {@link
        // maxVisibleRows} accounts, resize the list so that only {@link
        // maxVisibleRows}
        // accounts and part of the next one are visible.
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        int numAccounts = sheetItemListView.getAdapter().getItemCount();
        if (numAccounts < MIN_NUM_ACCOUNTS_FOR_SCROLL) {
            return maxHeightPx;
        }
        if (numAccounts > maxVisibleRows) {
            sheetItemListView.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            View accountRow = sheetItemListView.getChildAt(0);
            @Px int measuredHeight = sheetItemListView.getMeasuredHeight();
            @Px int desiredHeight = Math.round(accountRow.getMeasuredHeight() * maxVisibleRows);
            return maxHeightPx - measuredHeight + desiredHeight;
        }
        return maxHeightPx;
    }

    /**
     * Returns the height of the half state in passive mode when multiple IDPs are being shown.
     * Shows one account fully. If there are more, it shows the first 1.5 accounts to encourage
     * scrolling.
     *
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getDesiredPassiveModeMultiIdpSheetHeightPx() {
        if (!mCustomAccessibilityDelegateSet && mContentView != null) {
            // Add delegate so that the BottomSheet expands to full height if a11y focus occurs. We
            // could pass the item list instead but this is less disruptive since the focus shifts
            // when expandSheet occurs.
            ViewCompat.setAccessibilityDelegate(
                    mContentView,
                    new ExpandOnFocusAccessibilityDelegate(this, mBottomSheetController));
            mCustomAccessibilityDelegateSet = true;
        }
        return getDesiredSheetHeight(
                MAX_VISIBLE_ACCOUNTS_PASSIVE_MODE_MULTI_IDP, getMaximumSheetHeightPx());
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
    public float getFullHeightRatio() {
        if (mRpMode == RpMode.PASSIVE && !mIsMultipleIdps) {
            computeAndUpdateAccountListHeightForPassiveSingleIdp();
        }
        // WRAP_CONTENT would be the right fit but this disables the HALF state and this does not
        // work properly when we transition from a multi IDP UI to a single IDP UI, for unknown
        // reasons.
        return Math.min(getMaximumSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        if (mRpMode == RpMode.PASSIVE) {
            if (!mIsMultipleIdps || !mIsPassiveModeHalfHeightEnabled) {
                return HeightMode.DISABLED;
            }
            return Math.min(
                            getDesiredPassiveModeMultiIdpSheetHeightPx(),
                            mBottomSheetController.getContainerHeight())
                    / (float) mBottomSheetController.getContainerHeight();
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
    public @NonNull String getSheetContentDescription(Context context) {
        return context.getString(R.string.account_selection_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.account_selection_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.account_selection_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.account_selection_sheet_closed;
    }
}
