// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Implements the content for the AtMemory Flyout. */
@NullMarked
class AtMemoryFlyoutContent implements BottomSheetContent {
    private final View mContentView;

    AtMemoryFlyoutContent(Context context) {
        mContentView = new FrameLayout(context);
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
        return 0.5f;
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
        // TODO(crbug.com/505257277): Implement a string.
        return "";
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/505257277): Provide a proper accessibility string for the half height state.
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/505257277): Provide a proper accessibility string for the full height state.
        return 0;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/505257277): Provide a proper accessibility string for the closed state.
        return 0;
    }
}
