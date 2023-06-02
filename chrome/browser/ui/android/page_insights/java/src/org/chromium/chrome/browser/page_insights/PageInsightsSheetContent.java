// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

public class PageInsightsSheetContent implements BottomSheetContent {
    /** Ratio of the height when in full mode. */
    private static final float FULL_HEIGHT_RATIO = 0.9f;

    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;

    /**
     * Constructor.
     * @param context An Android context.
     */
    public PageInsightsSheetContent(Context context) {
        mToolbarView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.page_insights_sheet_toolbar, null);
        mSheetContentView = new FrameLayout(context);
    }

    @Override
    public boolean hasCustomLifecycle() {
        // Lifecycle is controlled by triggering logic.
        return true;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // We use the standard scrim that open when going beyond the peeking state.
        return false;
    }

    @Override
    public boolean hideOnScroll() {
        // PIH scrolls away in sync with tab scroll.
        return true;
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPeekHeight() {
        // TODO(b/282739536): Find the right peeking height value from the feed view dimension.
        return 400;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return FULL_HEIGHT_RATIO;
    }

    @Override
    public int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // Swiping down hard/tapping on scrim closes the sheet.
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.page_insights_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.page_insights_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.page_insights_sheet_closed;
    }
}
