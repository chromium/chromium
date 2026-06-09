// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Implements the content for the @memory bottom sheet. */
@NullMarked
class AtMemoryBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final BottomSheetController mBottomSheetController;

    AtMemoryBottomSheetContent(View contentView, BottomSheetController bottomSheetController) {
        mContentView = contentView;
        mBottomSheetController = bottomSheetController;
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
    public boolean hasCustomScrimLifecycle() {
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
    public float getHalfHeightRatio() {
        return Math.min(
                getSheetContentHeight() / (float) mBottomSheetController.getContainerHeight(),
                0.5f);
    }

    @Override
    public float getFullHeightRatio() {
        return 1.0f;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        // TODO(crbug.com/502801668): Implement a string.
        return "";
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/502801668): Implement a string.
        return R.string.done;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/502801668): Implement a string.
        return R.string.done;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/502801668): Implement a string.
        return R.string.done;
    }

    // Measures the content height to achieve a wrap-content effect for the bottom sheet.
    // Using HeightMode.WRAP_CONTENT for getFullHeightRatio() disables the view from being
    // expandable.
    private float getSheetContentHeight() {
        mContentView.measure(
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getContainerWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getContainerHeight(), MeasureSpec.AT_MOST));
        return mContentView.getMeasuredHeight();
    }
}
