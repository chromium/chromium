// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.navigation_pane;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

/** Custom view for the navigation pane item. */
@NullMarked
public class NavigationPaneItemView extends LinearLayout {
    private TextView mTitle;
    private ImageView mIcon;

    /** Constructor for inflating from XML. */
    public NavigationPaneItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.title);
        mIcon = findViewById(R.id.icon);
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }

    void setIcon(@Nullable Drawable icon) {
        mIcon.setImageDrawable(icon);
    }
}
