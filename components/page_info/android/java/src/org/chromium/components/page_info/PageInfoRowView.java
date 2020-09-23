// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * View showing an icon, title and subtitle for a page info row.
 */
public class PageInfoRowView extends FrameLayout {
    /**  Parameters to configure the row view. */
    public static class ViewParams {
        public boolean visible;
        public @DrawableRes int iconResId;
        public String title;
        public String subtitle;
        public Runnable clickCallback;
    }

    private final ImageView mIcon;
    private final TextView mTitle;
    private final TextView mSubtitle;

    public PageInfoRowView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.page_info_row, this, true);
        mIcon = findViewById(R.id.page_info_row_icon);
        mTitle = findViewById(R.id.page_info_row_title);
        mSubtitle = findViewById(R.id.page_info_row_subtitle);
    }

    public void setParams(ViewParams params) {
        setVisibility(params.visible ? VISIBLE : GONE);
        mIcon.setImageResource(params.iconResId);
        mTitle.setText(params.title);
        mTitle.setVisibility(params.title != null ? VISIBLE : GONE);
        updateSubtitle(params.subtitle);
        if (params.clickCallback != null) {
            setClickable(true);
            setFocusable(true);
            getChildAt(0).setOnClickListener((v) -> params.clickCallback.run());
        }
    }

    public void updateSubtitle(String subtitle) {
        mSubtitle.setText(subtitle);
        mSubtitle.setVisibility(subtitle != null ? VISIBLE : GONE);
    }
}
