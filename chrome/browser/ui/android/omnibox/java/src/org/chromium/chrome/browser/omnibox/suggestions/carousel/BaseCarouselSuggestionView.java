// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;
import android.view.KeyEvent;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * View for Carousel Suggestions.
 */
@MockedInTests
public class BaseCarouselSuggestionView extends RecyclerView {
    private final BaseCarouselSuggestionSelectionManager mSelectionManager;

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
        getResources().getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding);
        setPaddingRelative(0, topPadding, getPaddingEnd(), bottomPadding);

        mSelectionManager = new BaseCarouselSuggestionSelectionManager(getLayoutManager());
        addOnChildAttachStateChangeListener(mSelectionManager);

        setAdapter(adapter);
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
}
