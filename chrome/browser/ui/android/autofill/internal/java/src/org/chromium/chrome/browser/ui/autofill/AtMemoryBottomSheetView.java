// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** View wrapper for the @memory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetView {
    private final View mContentView;
    private final RecyclerView mRecyclerView;

    public AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);

        mRecyclerView = mContentView.findViewById(R.id.suggestions_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
        mRecyclerView.addItemDecoration(new AtMemoryDividerItemDecoration(context));

        initializeSearchQueryInput();
    }

    private void initializeSearchQueryInput() {
        EditText searchInput = mContentView.findViewById(R.id.search_query_input);
        View clearButton = mContentView.findViewById(R.id.clear_search_button);

        clearButton.setOnClickListener(v -> clearSearchText());
        searchInput.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        clearButton.setVisibility(s.length() > 0 ? View.VISIBLE : View.GONE);
                    }

                    @Override
                    public void afterTextChanged(Editable s) {}
                });
    }

    public View getContentView() {
        return mContentView;
    }

    public void setRecyclerViewAdapter(Adapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    public void focusSearchArea() {
        View searchInput = mContentView.findViewById(R.id.search_query_input);
        assert searchInput != null;
        // TODO(crbug.com/512802813): Fix cursor not blinking on subsequent openings of the bottom
        // sheet.
        searchInput.requestFocus();
        KeyboardUtils.showKeyboard(searchInput);
    }

    public void clearSearchText() {
        View searchInput = mContentView.findViewById(R.id.search_query_input);
        assert searchInput instanceof EditText;
        ((EditText) searchInput).setText("");
    }

    /** Draws a divider line below each item in the list except for the last item. */
    private static class AtMemoryDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final Drawable mDivider;
        private final int mDividerHeight;

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
                        || position == adapter.getItemCount() - 1) {
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
            if (position == RecyclerView.NO_POSITION || position == adapter.getItemCount() - 1) {
                outRect.set(0, 0, 0, 0);
            } else {
                outRect.set(0, 0, 0, mDividerHeight);
            }
        }
    }
}
