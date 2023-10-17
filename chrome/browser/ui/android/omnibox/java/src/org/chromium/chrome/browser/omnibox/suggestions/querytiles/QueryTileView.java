// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/** The view for a QueryTile. */
public class QueryTileView extends FrameLayout {
    private final RoundedCornerImageView mThumbnail;
    private final TextView mTitle;
    private @Nullable Runnable mOnFocusViaSelectionListener;

    public QueryTileView(Context context) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.query_tile_view, this, true);

        mThumbnail = findViewById(R.id.thumbnail);
        mTitle = findViewById(R.id.title);
    }

    @Override
    public void setSelected(boolean isSelected) {
        super.setSelected(isSelected);
        if (isSelected && mOnFocusViaSelectionListener != null) {
            mOnFocusViaSelectionListener.run();
        }
    }

    /**
     * Report View focused state when it's either focused or selected. Allows rendering view focused
     * state when view is highlighted via the means of keyboard navigation.
     */
    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }

    /* package */ void setImage(Drawable d) {
        mThumbnail.setImageDrawable(d);
    }

    /* package */ void setTitle(String t) {
        mTitle.setText(t);
    }

    /* package */ void setOnFocusViaSelectionListener(Runnable listener) {
        mOnFocusViaSelectionListener = listener;
    }
}
