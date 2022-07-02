// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.modelutil.PropertyKey;

/**
 * This view renders content that gets displayed inside the bottom sheet. This
 * is a simple container for the view which is the current best practice for
 * bottom sheet content.
 */
public class AccountSelectionBottomSheetContent implements BottomSheetContent {
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

    public void focusForAccessibility(PropertyKey focusItem) {
        // {@link mContentView} is null for some tests.
        if (mContentView == null) return;

        View focusView = null;
        if (focusItem == ItemProperties.HEADER) {
            focusView = mContentView.findViewById(R.id.header_title);
        } else if (focusItem == ItemProperties.CONTINUE_BUTTON) {
            focusView = mContentView.findViewById(R.id.account_selection_continue_btn);
        } else {
            assert false;
        }

        if (focusView != null) {
            focusView.requestFocus();
            focusView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
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
