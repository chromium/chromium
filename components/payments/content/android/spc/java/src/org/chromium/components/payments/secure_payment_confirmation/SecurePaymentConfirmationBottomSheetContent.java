// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.payments.R;

/** Implements the bottom sheet content for the secure payment confirmation bottom sheet. */
@NullMarked
/*package*/ public class SecurePaymentConfirmationBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final ScrollView mScrollView;

    /**
     * Creates the save card contents.
     *
     * @param contentView The bottom sheet content.
     * @param scrollView The view that scrolls the contents within the sheet on smaller screens.
     */
    /*package*/ SecurePaymentConfirmationBottomSheetContent(
            View contentView, ScrollView scrollView) {
        mContentView = contentView;
        mScrollView = scrollView;
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
        return mScrollView.getScrollY();
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public void destroy() {
        // In order to be able to know the reason for this bottom sheet being closed, the
        // BottomSheetObserver interface is used by the SPC bottom sheet observer class instead.
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(
                R.string.secure_payment_confirmation_authentication_sheet_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.secure_payment_confirmation_authentication_sheet_opened;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.secure_payment_confirmation_authentication_sheet_closed;
    }
}
