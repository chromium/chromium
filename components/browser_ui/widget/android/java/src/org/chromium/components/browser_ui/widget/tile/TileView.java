// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.tile;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/**
 * The view for a tile with icon and text.
 *
 * <p>Displays the title of the site beneath a large icon.
 */
@NullMarked
public class TileView extends FrameLayout {
    private ImageView mOfflineBadgeView;
    private ImageView mPinnedShortcutBadgeView;
    private TextView mTitleView;
    private @Nullable Runnable mOnFocusViaSelectionListener;
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
        mOfflineBadgeView = findViewById(R.id.offline_badge);
        mPinnedShortcutBadgeView = findViewById(R.id.pinned_shortcut_badge);
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
     * @param showPinnedShortcutBadge Whether to show the pinned shortcut badge.
     * @param icon The icon to display on the tile.
     * @param titleLines The number of text lines to use for the tile title.
     */
    public void initialize(
            String title,
            boolean showOfflineBadge,
            boolean showPinnedShortcutBadge,
            @Nullable Drawable icon,
            int titleLines) {
        setOfflineBadgeVisibility(showOfflineBadge);
        togglePinnedShortcutBadge(showPinnedShortcutBadge);
        setIconDrawable(icon);
        setTitle(title, titleLines);
    }

    /** Renders the icon or clears it from the view if the icon is null. */
    public void setIconDrawable(@Nullable Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    /** Applies or clears icon tint. */
    public void setIconTint(@Nullable ColorStateList color) {
        ImageViewCompat.setImageTintList(mIconView, color);
    }

    /** Shows or hides the offline badge to reflect the offline availability. */
    public void setOfflineBadgeVisibility(boolean isVisible) {
        mOfflineBadgeView.setVisibility(isVisible ? VISIBLE : GONE);
    }

    /**
     * Shows or hides the pinned shortcut badge to reflect whether the tile is a Custom Tile and
     * adjusts surrounding views to accommodate.
     */
    public void togglePinnedShortcutBadge(boolean isVisible) {
        if (isVisible) {
            // Recompute scaling on each call (instead of caching as static member) to be robust
            // against display size and font size changes -- this is relatively cheap anyway.
            Matrix matrix = new Matrix();

            // The interior "pin" shape of ic_keep_24dp.xml is at (6dp, 3dp) - (18dp, 23dp), i.e.,
            // size of 12dp x 20dp. Crop and map to (0sp, 0sp) - (6sp, 10sp) of the badge.
            float dp = getResources().getDisplayMetrics().density;
            float sp = getResources().getDisplayMetrics().scaledDensity;
            RectF src = new RectF(6f * dp, 3f * dp, 18f * dp, 23f * dp);
            RectF dst = new RectF(0f, 0f, 6f * sp, 10f * sp);
            matrix.setRectToRect(src, dst, Matrix.ScaleToFit.FILL);
            mPinnedShortcutBadgeView.setImageMatrix(matrix);
        }

        mPinnedShortcutBadgeView.setVisibility(isVisible ? VISIBLE : GONE);

        // When badge is shown, horizontally align title text to start so that spacing between is
        // consistent. When badge is hidden, horizontally align text to center so that when title
        // text is maximized with ellipsis is shown, the interior text will truly be centered.
        mTitleView.setGravity(
                Gravity.TOP | (isVisible ? Gravity.START : Gravity.CENTER_HORIZONTAL));
    }

    /** Sets the title text and number lines. */
    public void setTitle(String title, int titleLines) {
        mTitleView.setLines(titleLines);
        mTitleView.setText(title);
    }

    /** Sets the max number of lines taken by the title. */
    public void setTitleMaxLines(int maxLines) {
        mTitleView.setMaxLines(maxLines);
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
    public TextView getTitleView() {
        return mTitleView;
    }

    @Override
    public void setSelected(boolean isSelected) {
        super.setSelected(isSelected);
        if (isSelected && mOnFocusViaSelectionListener != null) {
            mOnFocusViaSelectionListener.run();
        }
    }

    /** Returns whether the tile can be moved using drag-and-drop. */
    public boolean isDraggable() {
        return false;
    }
}
