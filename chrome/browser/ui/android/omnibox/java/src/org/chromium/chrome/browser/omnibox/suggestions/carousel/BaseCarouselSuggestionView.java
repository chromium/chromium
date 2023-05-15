// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * View for Carousel Suggestions.
 */
public class BaseCarouselSuggestionView extends LinearLayout {
    private final HeaderView mHeader;
    private final RecyclerView mRecyclerView;
    private final BaseCarouselSuggestionSelectionManager mSelectionManager;
    private int mItemSpacingPx;

    /**
     * Constructs a new carousel suggestion view.
     *
     * @param context Current context.
     */
    public BaseCarouselSuggestionView(Context context, SimpleRecyclerViewAdapter adapter) {
        super(context);
        setClickable(false);
        setFocusable(false);
        setOrientation(VERTICAL);
        final int verticalPad =
                getResources().getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding);
        setPaddingRelative(0, verticalPad, 0, verticalPad);

        mHeader = new HeaderView(context);
        mHeader.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mHeader.setVisibility(View.GONE);
        addView(mHeader);

        mRecyclerView = new RecyclerView(context);
        mRecyclerView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mRecyclerView.setFocusable(true);
        mRecyclerView.setFocusableInTouchMode(true);
        mRecyclerView.setItemAnimator(null);
        mRecyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        mRecyclerView.setClipToPadding(false);
        int startPadding = OmniboxResourceProvider.getSideSpacing(context);
        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            startPadding -= context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_carousel_start_padding_reduction);
        }
        mRecyclerView.setPaddingRelative(startPadding, mRecyclerView.getPaddingTop(),
                mRecyclerView.getPaddingEnd(), mRecyclerView.getPaddingBottom());

        mSelectionManager =
                new BaseCarouselSuggestionSelectionManager(mRecyclerView.getLayoutManager());
        mRecyclerView.addOnChildAttachStateChangeListener(mSelectionManager);

        mRecyclerView.addItemDecoration(new ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                outRect.left = 0;
                outRect.right = mItemSpacingPx;
            }
        });

        mRecyclerView.setAdapter(adapter);
        addView(mRecyclerView);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mSelectionManager.selectNextItem();
            return true;
        } else if ((isRtl && KeyNavigationUtil.isGoRight(event))
                || (!isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mSelectionManager.selectPreviousItem();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean isSelected) {
        if (isSelected) {
            mSelectionManager.setSelectedItem(0, true);
        } else {
            mSelectionManager.setSelectedItem(RecyclerView.NO_POSITION, false);
        }
    }

    /** @return Header TextView element. */
    TextView getHeaderTextView() {
        return mHeader;
    }

    /** @return Header element. */
    View getHeaderView() {
        return mHeader;
    }

    /** @return Adapter used with the embedded RecyclerView. */
    SimpleRecyclerViewAdapter getAdapter() {
        return (SimpleRecyclerViewAdapter) mRecyclerView.getAdapter();
    }

    /** @return Recycler view used by the Carousel suggestion. */
    public RecyclerView getRecyclerViewForTest() {
        return mRecyclerView;
    }

    /**
     * Applies a new item spacing to the carousel.
     *
     * @param itemSpacingPx The requested item spacing, expressed in Pixels.
     */
    public void setItemSpacingPx(int itemSpacingPx) {
        mItemSpacingPx = itemSpacingPx;
        ViewUtils.requestLayout(mRecyclerView, "BaseCarouselSuggestionView.setItemSpacingPx");
    }

    /**
     * Set the carousel to have horizontal fade effect.
     *
     * @param enableFade whether we should enable horizontal fade.
     */
    public void setCarouselHorizontalFade(boolean enableFade) {
        mRecyclerView.setHorizontalFadingEdgeEnabled(enableFade);
    }

    /**
     * Set the recycler view pool to the carousel view to reduce extra image fetching and jackiness
     * on carousel rendering.
     *
     * @param recycledViewPool the recycled view pool to assign to the recycler view.
     */
    void setCarouselRecycledViewPool(RecycledViewPool recycledViewPool) {
        // TODO(rongtan): Investigate why null assignment causes crashes in Recycler View.
        if (recycledViewPool != null) {
            mRecyclerView.setRecycledViewPool(recycledViewPool);
        }
    }
}
