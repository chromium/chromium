// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.navigation_pane;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

/** Custom view for the navigation pane header. */
@NullMarked
public class NavigationPaneHeaderView extends LinearLayout {
    private TextView mTitle;

    /** Constructor for inflating from XML. */
    public NavigationPaneHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.title);
    }

    /**
     * Sets the title text displayed in the header view.
     *
     * @param title The title text to display.
     */
    void setTitle(String title) {
        mTitle.setText(title);
    }
}
