// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LevelListDrawable;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.R;

/**
 * Implementation of SelectableItemViewBase which requires the caller to inflate their own view.
 * Useful for when you need to diverge from the built-in layout of SelectableItemView, but want
 * the checkmark functionality. Backgrounds used for the #iconView should be instances of
 * LevelListDrawable to properly interact with the checkmark behavior, see list_item_icon_modern_bg
 * for an example of how to setup a new drawable.
 *
 * @param <E> The type of the item associated with this SelectableItemViewBase.
 */
public abstract class CheckableSelectableItemView<E> extends SelectableItemViewBase<E> {
    private final AnimatedVectorDrawableCompat mCheckDrawable;

    /** The color state list for the start icon view when the item is selected. */
    private ColorStateList mIconSelectedColorList;

    /** Drawable for the start icon */
    private @Nullable Drawable mIconDrawable;

    /** Constructor for inflating from XML. */
    public CheckableSelectableItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIconSelectedColorList =
                ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorInverse(context));
        mCheckDrawable =
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.ic_check_googblue_24dp_animated);
    }

    // Abstract methods.

    /** Returns the icon view used to show the checkmark when selected. */
    protected abstract ImageView getIconView();

    /**
     * Returns the default tint for the icon view, can be null. Will be swapped with a different
     * tint when the checkmark is shown.
     */
    protected abstract @Nullable ColorStateList getDefaultIconTint();

    /**
     * Returns the selected level for the icon view background. This level is used when the
     * drawable set by #setIconDrawable is displayed. The background for the ImageView returned
     * by #getIconView should be a LevelListDrawable that supports both the default and selected
     * state.
     */
    protected abstract int getDefaultLevel();

    /**
     * Returns the selected level for the icon view background. This level is used when the
     * checkmark is displayed. The background for the ImageView returned by #getIconView should
     * be a LevelListDrawable that supports both the default and selected state.
     */
    protected abstract int getSelectedLevel();

    /**
     * Set drawable for the start icon view. Note that you may need to use this method instead of
     * mIconView#setImageDrawable to ensure icon view is correctly set in selection mode.
     */
    public void setIconDrawable(Drawable iconDrawable) {
        mIconDrawable = iconDrawable;
        updateView(false);
    }

    /** Returns the drawable set for the start icon view, if any. */
    public Drawable getIconDrawable() {
        return mIconDrawable;
    }

    @Override
    protected void updateView(boolean animate) {
        ImageView iconView = getIconView();
        if (iconView == null) return;
        Drawable iconViewBackground = iconView.getBackground();
        assert iconViewBackground instanceof LevelListDrawable;

        boolean levelMatches;
        boolean levelChangeSuccess;
        if (isChecked()) {
            int level = getSelectedLevel();
            levelMatches = level == iconViewBackground.getLevel();
            levelChangeSuccess = iconViewBackground.setLevel(level);
            iconView.setImageDrawable(mCheckDrawable);
            ImageViewCompat.setImageTintList(iconView, mIconSelectedColorList);
            if (animate) mCheckDrawable.start();
        } else {
            int level = getDefaultLevel();
            levelMatches = level == iconViewBackground.getLevel();
            levelChangeSuccess = iconViewBackground.setLevel(level);
            iconView.setImageDrawable(mIconDrawable);
            ImageViewCompat.setImageTintList(iconView, getDefaultIconTint());
        }
        assert levelMatches || levelChangeSuccess;
    }

    public void endAnimationsForTests() {
        mCheckDrawable.stop();
    }
}
