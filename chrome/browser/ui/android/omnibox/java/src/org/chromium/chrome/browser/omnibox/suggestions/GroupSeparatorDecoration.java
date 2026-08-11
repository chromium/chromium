// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.GroupSeparatorType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Draws a gap between groups of suggestions. */
@NullMarked
public class GroupSeparatorDecoration extends RecyclerView.ItemDecoration {
    private final int mSeparatorHeight;
    private final int mHorizontalPadding;
    private final Paint mPaint = new Paint();

    public GroupSeparatorDecoration(Context context) {
        Resources res = context.getResources();
        mSeparatorHeight =
                res.getDimensionPixelSize(R.dimen.divider_height)
                        + res.getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_divider_line_vertical_padding);
        mHorizontalPadding =
                res.getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_list_divider_line_horizontal_padding);
        mPaint.setColor(SemanticColorUtils.getDividerColor(context));
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        if (parent.getChildViewHolder(view)
                instanceof SimpleRecyclerViewAdapter.ViewHolder suggestionViewHolder) {
            PropertyModel model = suggestionViewHolder.model;
            if (model == null) return;
            if (model.get(SuggestionCommonProperties.GROUP_SEPARATOR_TYPE)
                    == GroupSeparatorType.NONE) {
                return;
            }
            outRect.top = mSeparatorHeight;
        }
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        int left = parent.getPaddingLeft() + mHorizontalPadding;
        int right = parent.getWidth() - parent.getPaddingRight() - mHorizontalPadding;

        int childCount = parent.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            if (parent.getChildViewHolder(child)
                    instanceof SimpleRecyclerViewAdapter.ViewHolder suggestionViewHolder) {
                PropertyModel model = suggestionViewHolder.model;
                if (model != null
                        && model.get(SuggestionCommonProperties.GROUP_SEPARATOR_TYPE)
                                == GroupSeparatorType.LINE) {
                    RecyclerView.LayoutParams lp =
                            (RecyclerView.LayoutParams) child.getLayoutParams();
                    int centerY = child.getTop() - lp.topMargin - mSeparatorHeight / 2;
                    c.drawRect(left, centerY, right, centerY + 1, mPaint);
                }
            }
        }
    }
}
