// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * The {@link BottomSheetContent} for Fast Checkout.
 */
public class FastCheckoutSheetContent implements BottomSheetContent {
    private final View mContentView;

    /**
     * Constructs a FastCheckoutSheetContent which creates, modifies, and shows the bottom sheet.
     */
    FastCheckoutSheetContent(View contentView) {
        mContentView = contentView;
    }

    /**
     * Sets the screen to show on the bottom sheet.
     * @param screenType A {@link ScreenType} specifying the screen to show.
     */
    void updateCurrentScreen(int screenType) {
        // TODO(crbug.com/1334642): Implement.
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        // TODO(crbug.com/1334642): Implement.
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        // TODO(crbug.com/1334642): Implement.
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
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // TODO(crbug.com/1334642): Implement.
        return HeightMode.DEFAULT;
    }

    @Override
    public float getHalfHeightRatio() {
        // TODO(crbug.com/1334642): Implement.
        return BottomSheetContent.HeightMode.DEFAULT;
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
        return R.string.fast_checkout_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.fast_checkout_sheet_full_height;
    }
}
