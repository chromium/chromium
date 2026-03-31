// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Draws a gap between groups of suggestions. */
@NullMarked
public class GroupSeparatorDecoration extends RecyclerView.ItemDecoration {
    private final int mSeparatorHeight;

    public GroupSeparatorDecoration(Context context) {
        Resources res = context.getResources();
        mSeparatorHeight =
                res.getDimensionPixelSize(R.dimen.divider_height)
                        + res.getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_divider_line_padding);
    }

    @Override
    public void getItemOffsets(
            @NonNull Rect outRect,
            @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state) {
        if (parent.getChildViewHolder(view)
                instanceof SimpleRecyclerViewAdapter.ViewHolder suggestionViewHolder) {
            PropertyModel model = suggestionViewHolder.model;
            if (model != null && model.get(SuggestionCommonProperties.SHOW_GROUP_SEPARATOR)) {
                outRect.top = mSeparatorHeight;
            }
        }
    }
}
