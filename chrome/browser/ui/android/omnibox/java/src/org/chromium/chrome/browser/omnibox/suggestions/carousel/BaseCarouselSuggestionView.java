// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;
import android.view.KeyEvent;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.omnibox.suggestions.base.DynamicSpacingRecyclerViewItemDecoration;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** View for Carousel Suggestions. */
@MockedInTests
public class BaseCarouselSuggestionView extends RecyclerView {
    private RecyclerViewSelectionController mSelectionController;
    private DynamicSpacingRecyclerViewItemDecoration mDecoration;

    /**
     * Constructs a new carousel suggestion view.
     *
     * @param context Current context.
     */
    public BaseCarouselSuggestionView(Context context, SimpleRecyclerViewAdapter adapter) {
        super(context);

        setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        setFocusable(true);
        setFocusableInTouchMode(true);
        setItemAnimator(null);
        setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));

        int topPadding = OmniboxResourceProvider.getCarouselTopPadding(context);
        int bottomPadding = OmniboxResourceProvider.getCarouselBottomPadding(context);
        setPaddingRelative(0, topPadding, getPaddingEnd(), bottomPadding);

        mSelectionController = new RecyclerViewSelectionController(getLayoutManager());
        addOnChildAttachStateChangeListener(mSelectionController);

        int initialSpacing =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                        ? OmniboxResourceProvider.getHeaderStartPadding(context)
                                - getResources().getDimensionPixelSize(R.dimen.tile_view_padding)
                        : OmniboxResourceProvider.getSideSpacing(context);
        int baseSpacing =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_carousel_suggestion_minimum_item_spacing);
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(this, initialSpacing, baseSpacing / 2);
        addItemDecoration(mDecoration);

        setAdapter(adapter);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mSelectionController.selectNextItem();
            return true;
        } else if ((isRtl && KeyNavigationUtil.isGoRight(event))
                || (!isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mSelectionController.selectPreviousItem();
            return true;
        } else if (KeyNavigationUtil.isEnter(event)) {
            var tile = mSelectionController.getSelectedView();
            if (tile != null) return tile.performClick();
        }
        return superOnKeyDown(keyCode, event);
    }

    /**
     * Proxy calls to super.onKeyDown; call exposed for testing purposes. There is no way to detect
     * calls to super using robolectric.
     */
    @CheckDiscard("Should be inlined except for testing")
    @VisibleForTesting
    public boolean superOnKeyDown(int keyCode, KeyEvent event) {
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean isSelected) {
        if (isSelected) {
            mSelectionController.setSelectedItem(0, true);
        } else {
            mSelectionController.setSelectedItem(RecyclerView.NO_POSITION, false);
        }
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mDecoration.notifyViewMeasuredSizeChanged();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    /* package */ void setSelectionControllerForTesting(
            RecyclerViewSelectionController controller) {
        removeOnChildAttachStateChangeListener(mSelectionController);
        mSelectionController = controller;
        addOnChildAttachStateChangeListener(mSelectionController);
    }

    /* package */ DynamicSpacingRecyclerViewItemDecoration getItemDecoration() {
        return mDecoration;
    }

    @VisibleForTesting
    /* package */ void setItemDecorationForTesting(
            DynamicSpacingRecyclerViewItemDecoration decoration) {
        removeItemDecoration(mDecoration);
        mDecoration = decoration;
        addItemDecoration(mDecoration);
    }
}
