// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties.ItemType;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class AtMemoryHomeView extends LinearLayout {
    private AtMemorySearchBarView mSearchBarView;
    private RecyclerView mRecyclerView;
    private View mNoticeContainer;
    private View mNoticeOkButton;
    private View mNoticeSettingsLink;

    public AtMemoryHomeView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mSearchBarView = findViewById(R.id.search_query_input_container);
        mRecyclerView = findViewById(R.id.suggestions_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        mNoticeContainer = findViewById(R.id.notice_container);
        mNoticeOkButton = findViewById(R.id.notice_ok_button);
        mNoticeSettingsLink = findViewById(R.id.notice_manage_settings_link);
    }

    public void setUpSheetItems(ModelList items) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(items);

        adapter.registerType(
                ItemType.SUGGESTION,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_suggestion_item),
                AtMemoryBottomSheetViewBinder::bindSuggestionItemView);

        adapter.registerType(
                ItemType.ZERO_STATE,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_zero_state_item),
                (model, view, propertyKey) -> {});

        mRecyclerView.setAdapter(adapter);
        mRecyclerView.addItemDecoration(new AtMemoryDividerItemDecoration(getContext()));
    }

    public void focusSearchArea() {
        mSearchBarView.focusSearchArea();
    }

    public void clearSearchText() {
        mSearchBarView.clearSearchText();
    }

    public void hideKeyboardAndClearFocus() {
        mSearchBarView.hideKeyboardAndClearFocus();
    }

    public void setNoticeVisible(boolean visible) {
        mNoticeContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    public void setNoticeOkClickListener(Runnable listener) {
        mNoticeOkButton.setOnClickListener(v -> listener.run());
    }

    public void setNoticeSettingsClickListener(Runnable listener) {
        mNoticeSettingsLink.setOnClickListener(v -> listener.run());
    }

    public void setSearchBarDelegate(AtMemorySearchBarView.Delegate delegate) {
        mSearchBarView.setDelegate(delegate);
    }

    public boolean searchHasFocus() {
        return mSearchBarView.searchHasFocus();
    }

    public void setIsLoading(boolean isLoading) {
        mSearchBarView.setIsLoading(isLoading);
    }

    public void setShowSuggestionsBackground(boolean showBackground) {
        if (showBackground) {
            mRecyclerView.setBackgroundResource(R.drawable.at_memory_suggestions_bg);
        } else {
            mRecyclerView.setBackground(null);
        }
    }

    // TODO(crbug.com/513146609): Reuse ItemDividerBase instead of custom ItemDecoration.
    /** Draws a divider line below each item in the list except for the last item. */
    private static class AtMemoryDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final Drawable mDivider;
        private final @Px int mDividerHeight;

        public AtMemoryDividerItemDecoration(Context context) {
            mDivider = new ColorDrawable(SemanticColorUtils.getDefaultBgColor(context));
            mDividerHeight =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.at_memory_bottom_sheet_divider_height);
        }

        @Override
        public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
            Adapter adapter = parent.getAdapter();
            if (adapter == null) return;

            int left = parent.getPaddingLeft();
            int right = parent.getWidth() - parent.getPaddingRight();
            int childCount = parent.getChildCount();

            for (int i = 0; i < childCount; i++) {
                View child = parent.getChildAt(i);
                int position = parent.getChildAdapterPosition(child);
                if (position == RecyclerView.NO_POSITION
                        || position == adapter.getItemCount() - 1
                        || shouldSkipItemType(adapter.getItemViewType(position))) {
                    continue;
                }

                RecyclerView.LayoutParams params =
                        (RecyclerView.LayoutParams) child.getLayoutParams();
                int top = child.getBottom() + params.bottomMargin;
                int bottom = top + mDividerHeight;

                mDivider.setBounds(left, top, right, bottom);
                mDivider.draw(c);
            }
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            Adapter adapter = parent.getAdapter();
            if (adapter == null) return;

            int position = parent.getChildAdapterPosition(view);
            if (position == RecyclerView.NO_POSITION
                    || position == adapter.getItemCount() - 1
                    || shouldSkipItemType(adapter.getItemViewType(position))) {
                outRect.set(0, 0, 0, 0);
            } else {
                outRect.set(0, 0, 0, mDividerHeight);
            }
        }

        private boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.ZERO_STATE:
                    return true;
                case ItemType.SUGGESTION:
                    return false;
                default:
                    return false;
            }
        }
    }
}
