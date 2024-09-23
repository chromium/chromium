// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.tile;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/**
 * The view for a tile with icon and text.
 *
 * Displays the title of the site beneath a large icon.
 */
public class TileView extends FrameLayout {
    private ImageView mBadgeView;
    private TextView mTitleView;
    private Runnable mOnFocusViaSelectionListener;
    private RoundedCornerOutlineProvider mRoundingOutline;
    protected ImageView mIconView;
    protected View mIconBackgroundView;

    /** Constructor for inflating from XML. */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * See {@link View#onFinishInflate} for details.
     *
     * Important:
     * This method will never be called when the layout is inflated from a <merge> fragment.
     * LayoutInflater explicitly avoids invoking this method with merge fragments.
     * Make sure your layouts explicitly reference the TileView as the top component, rather
     * than deferring to <merge> tags.
     */
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.tile_view_icon);
        mBadgeView = findViewById(R.id.offline_badge);
        mTitleView = findViewById(R.id.tile_view_title);
        mIconBackgroundView = findViewById(R.id.tile_view_icon_background);
        mRoundingOutline = new RoundedCornerOutlineProvider();
        mIconView.setOutlineProvider(mRoundingOutline);
        mIconView.setClipToOutline(true);
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

    /** Renders the icon or clears it from the view if the icon is null. */
    public void setIconDrawable(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    /** Applies or clears icon tint. */
    public void setIconTint(ColorStateList color) {
        ImageViewCompat.setImageTintList(mIconView, color);
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

    /** Specify the handler that will be invoked when this tile is highlighted by the user. */
    void setOnFocusViaSelectionListener(Runnable listener) {
        mOnFocusViaSelectionListener = listener;
    }

    /** Sets the radius used to round the image content. */
    void setRoundingRadius(int radius) {
        mRoundingOutline.setRadius(radius);
    }

    /** Retrieves the radius used to round the image content. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    int getRoundingRadiusForTesting() {
        return mRoundingOutline.getRadiusForTesting();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public @NonNull TextView getTitleView() {
        return mTitleView;
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
        return super.isFocused() || isSelected();
    }
}
