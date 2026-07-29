// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties.ItemType;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.ItemDividerBase;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class AtMemoryHomeView extends LinearLayout {
    private AtMemorySearchBarView mSearchBarView;
    private RecyclerView mRecyclerView;

    public AtMemoryHomeView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mSearchBarView = findViewById(R.id.search_query_input_container);
        mRecyclerView = findViewById(R.id.suggestions_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        // Disable animations to prevent view flickering during frequent updates (e.g. search
        // affordance).
        mRecyclerView.setItemAnimator(null);
    }

    public void setUpSheetItems(ModelList items) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(items);

        adapter.registerType(
                ItemType.SUGGESTION,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_suggestion_item),
                AtMemoryBottomSheetViewBinder::bindSuggestionItemView);

        adapter.registerType(
                ItemType.SUGGESTION_WITH_NO_BACKGROUND,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_suggestion_item),
                AtMemoryBottomSheetViewBinder::bindSuggestionItemView);

        adapter.registerType(
                ItemType.ILLUSTRATION_CARD,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_illustration_card_item),
                AtMemoryBottomSheetViewBinder::bindIllustrationCardItemView);

        adapter.registerType(
                ItemType.NOTICE,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_notice_item),
                AtMemoryBottomSheetViewBinder::bindNoticeItemView);

        adapter.registerType(
                ItemType.TEXT_WITH_CLICKABLE_LINK,
                new LayoutViewBuilder<>(
                        R.layout.at_memory_bottom_sheet_text_with_clickable_link_item),
                AtMemoryBottomSheetViewBinder::bindTextWithClickableLinkView);

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

    public void setSearchBarDelegate(AtMemorySearchBarView.Delegate delegate) {
        mSearchBarView.setDelegate(delegate);
    }

    public boolean searchHasFocus() {
        return mSearchBarView.searchHasFocus();
    }

    public void setIsLoading(boolean isLoading) {
        mSearchBarView.setIsLoading(isLoading);
    }

    private static class AtMemoryDividerItemDecoration extends ItemDividerBase {
        public AtMemoryDividerItemDecoration(Context context) {
            super(context);
        }

        @Override
        protected boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.ILLUSTRATION_CARD:
                case ItemType.NOTICE:
                case ItemType.SUGGESTION_WITH_NO_BACKGROUND:
                case ItemType.TEXT_WITH_CLICKABLE_LINK:
                    return true;
                case ItemType.SUGGESTION:
                    return false;
                default:
                    return false;
            }
        }
    }
}
