// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.CREDIT_CARD_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.HOME_SCREEN;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.LocalizationUtils;

/** The {@link BottomSheetContent} for Fast Checkout. */
public class FastCheckoutSheetContent implements BottomSheetContent {
    private static final float MAX_VISIBLE_WHOLE_ADDRESSES = 2.5f;
    private static final float MAX_VISIBLE_WHOLE_CREDIT_CARDS = 3.5f;

    private final FastCheckoutSheetState mState;
    private final View mContentView;

    /**
     * Constructs a FastCheckoutSheetContent which creates, modifies, and shows the bottom sheet.
     */
    FastCheckoutSheetContent(FastCheckoutSheetState state, View contentView) {
        mState = state;
        mContentView = contentView;

        // Apply RTL layout changes for tests.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
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
        if (isAutofillProfileScreen() || isCreditCardScreen()) {
            RecyclerView recyclerView =
                    getContentView().findViewById(R.id.fast_checkout_detail_screen_recycler_view);
            return recyclerView.computeVerticalScrollOffset();
        }

        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
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
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        if (shouldWrapContent()) {
            return HeightMode.DISABLED;
        }
        return Math.min(getDesiredDetailSheetHeight(), mState.getContainerHeight())
                / (float) mState.getContainerHeight();
    }

    @Override
    public float getFullHeightRatio() {
        if (shouldWrapContent()) {
            return HeightMode.WRAP_CONTENT;
        }
        // This would ideally also be `WRAP_CONTENT` but that disables half height mode.
        // `mBottomSheetController.getContainerHeight()` is the height of the bottom sheet's
        // container, i.e. the screen.
        return Math.min(getBottomSheetHeight(), mState.getContainerHeight())
                / (float) mState.getContainerHeight();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.fast_checkout_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.fast_checkout_sheet_closed;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.fast_checkout_content_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.fast_checkout_content_description;
    }

    private boolean isHomeScreen() {
        return mState.getCurrentScreen() == HOME_SCREEN;
    }

    private boolean isAutofillProfileScreen() {
        return mState.getCurrentScreen() == AUTOFILL_PROFILE_SCREEN;
    }

    private boolean isCreditCardScreen() {
        return mState.getCurrentScreen() == CREDIT_CARD_SCREEN;
    }

    private float getBottomSheetHeight() {
        ViewGroup parent = (ViewGroup) getContentView().getParent();
        getContentView()
                .measure(
                        MeasureSpec.makeMeasureSpec(parent.getWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(parent.getHeight(), MeasureSpec.AT_MOST));
        return getContentView().getMeasuredHeight();
    }

    private boolean shouldWrapContent() {
        // Always got to FULL state in accessibility mode or when an external keyboard is connected
        // to allow scrolling the RecyclerView with e.g. side swipes or the Tab key.
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                || UiUtils.isHardwareKeyboardAttached()) {
            return true;
        }
        // If there are 1 or 2 Autofill profiles, it shows all items fully. For 3+ suggestions, it
        // shows the first 2.5 suggestions to encourage scrolling.
        boolean shouldWrapAutofillProfiles =
                isAutofillProfileScreen()
                        && mState.getNumOfAutofillProfiles() < MAX_VISIBLE_WHOLE_ADDRESSES;
        // If there are less than 4 credit cards, it shows all items fully. For 4+ suggestions, it
        // shows the first 3.5 suggestions to encourage scrolling.
        boolean shouldWrapCreditCards =
                isCreditCardScreen()
                        && mState.getNumOfCreditCards() < MAX_VISIBLE_WHOLE_CREDIT_CARDS;
        return isHomeScreen() || shouldWrapAutofillProfiles || shouldWrapCreditCards;
    }

    private int getDesiredDetailSheetHeight() {
        // TODO(crbug.com/40228235): Investigate measuring heights dynamically instead of using
        // hard-coded values.
        int height = getDimensionPixelSize(R.dimen.fast_checkout_detail_sheet_header_height);
        if (isAutofillProfileScreen()) {
            height +=
                    Math.round(
                            MAX_VISIBLE_WHOLE_ADDRESSES
                                    * getDimensionPixelSize(
                                            R.dimen
                                                    .fast_checkout_detail_sheet_height_single_address));
        } else {
            height +=
                    Math.round(
                            MAX_VISIBLE_WHOLE_CREDIT_CARDS
                                    * getDimensionPixelSize(
                                            R.dimen
                                                    .fast_checkout_detail_sheet_height_single_credit_card));
        }
        return height;
    }

    private int getDimensionPixelSize(int id) {
        return mContentView.getContext().getResources().getDimensionPixelSize(id);
    }
}
