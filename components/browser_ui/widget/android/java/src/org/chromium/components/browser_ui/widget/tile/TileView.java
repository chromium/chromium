// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.tile;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.components.browser_ui.widget.R;

/**
 * The view for a tile with icon and text.
 *
 * Displays the title of the site beneath a large icon.
 */
public class TileView extends FrameLayout {
    private ImageView mBadgeView;
    private TextView mTitleView;
    private Runnable mOnFocusViaSelectionListener;
    private ImageView mIconView;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.tile_view_icon);
        mBadgeView = findViewById(R.id.offline_badge);
        mTitleView = findViewById(R.id.tile_view_title);
    }

    /**
     * Initializes the view. Non-MVC components should call this immediately after inflation.
     *
     * @param title The title of the tile.
     * @param showOfflineBadge Whether to show the offline badge.
     * @param icon The icon to display on the tile.
     * @param titleLines The number of text lines to use for the tile title.
     */
    protected void initialize(
            String title, boolean showOfflineBadge, Drawable icon, int titleLines) {
        setOfflineBadgeVisibility(showOfflineBadge);
        setIconDrawable(icon);
        setTitle(title, titleLines);
    }

    /**
     * Renders the icon or clears it from the view if the icon is null.
     */
    public void setIconDrawable(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    /** Shows or hides the offline badge to reflect the offline availability. */
    protected void setOfflineBadgeVisibility(boolean showOfflineBadge) {
        mBadgeView.setVisibility(showOfflineBadge ? VISIBLE : GONE);
    }

    /** Sets the title text and number lines. */
    public void setTitle(String title, int titleLines) {
        mTitleView.setLines(titleLines);
        mTitleView.setText(title);
    }

    /**
     * Returns the ImageView for the icon.
     * This method is only to allow legacy code to continue to work. New code should not use this
     * method.
     * TODO(crbug.com/1179455): Clean up all usages and remove this method.
     */
    @Deprecated
    public ImageView getIconView() {
        return mIconView;
    }

    /** Specify the handler that will be invoked when this tile is highlighted by the user. */
    void setOnFocusViaSelectionListener(Runnable listener) {
        mOnFocusViaSelectionListener = listener;
    }

    @Override
    public void setSelected(boolean isSelected) {
        super.setSelected(isSelected);
        if (isSelected && mOnFocusViaSelectionListener != null) {
            mOnFocusViaSelectionListener.run();
        }
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }
}
