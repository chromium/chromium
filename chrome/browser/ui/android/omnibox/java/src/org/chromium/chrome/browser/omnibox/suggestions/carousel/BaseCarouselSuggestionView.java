// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;
import android.content.res.Configuration;
import android.view.KeyEvent;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.omnibox.suggestions.base.SpacingRecyclerViewItemDecoration;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** View for Carousel Suggestions. */
@MockedInTests
public class BaseCarouselSuggestionView extends RecyclerView {
    private RecyclerViewSelectionController mSelectionController;
    private SpacingRecyclerViewItemDecoration mDecoration;

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

        mSelectionController = new RecyclerViewSelectionController(getLayoutManager());
        mSelectionController.setCycleThroughNoSelection(true);
        addOnChildAttachStateChangeListener(mSelectionController);

        setAdapter(adapter);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_TAB && event.isShiftPressed()) {
            return mSelectionController.selectPreviousItem();
        } else if (keyCode == KeyEvent.KEYCODE_TAB) {
            return mSelectionController.selectNextItem();
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

    void resetSelection() {
        mSelectionController.setSelectedItem(RecyclerView.NO_POSITION);
    }

    @Override
    public void setSelected(boolean isSelected) {
        if (isSelected) {
            mSelectionController.setSelectedItem(0);
        } else {
            resetSelection();
        }
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (mDecoration.notifyViewSizeChanged(
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT,
                getMeasuredWidth(),
                getMeasuredHeight())) {
            invalidateItemDecorations();
        }
    }

    /* package */ void setSelectionControllerForTesting(
            RecyclerViewSelectionController controller) {
        removeOnChildAttachStateChangeListener(mSelectionController);
        mSelectionController = controller;
        addOnChildAttachStateChangeListener(mSelectionController);
    }

    /* package */ void setItemDecoration(SpacingRecyclerViewItemDecoration decoration) {
        if (mDecoration != null) {
            removeItemDecoration(mDecoration);
        }
        mDecoration = decoration;
        if (mDecoration != null) {
            addItemDecoration(mDecoration);
        }
    }
}
