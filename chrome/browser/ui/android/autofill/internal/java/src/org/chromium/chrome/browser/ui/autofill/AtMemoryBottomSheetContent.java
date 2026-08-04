// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Implements the content for the @memory bottom sheet. */
@NullMarked
class AtMemoryBottomSheetContent implements BottomSheetContent {
    private final AtMemoryBottomSheetView mView;
    private final BottomSheetController mBottomSheetController;

    AtMemoryBottomSheetContent(
            AtMemoryBottomSheetView view, BottomSheetController bottomSheetController) {
        mView = view;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public View getContentView() {
        return mView.getContentView();
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
        if (mView.searchHasFocus()) {
            return HeightMode.DISABLED;
        }
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

    @Override
    public @ColorInt int getSheetBackgroundColorOverride() {
        return SemanticColorUtils.getDefaultBgColor(getContentView().getContext());
    }

    // Measures the content height to achieve a wrap-content effect for the bottom sheet.
    private float getSheetContentHeight() {
        mView.getContentView()
                .measure(
                        MeasureSpec.makeMeasureSpec(
                                mBottomSheetController.getMaxSheetWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(
                                mBottomSheetController.getContainerHeight(), MeasureSpec.AT_MOST));
        return mView.getContentView().getMeasuredHeight();
    }
}
