// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatImageButton;
import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.widget.R;

/**
 * Default implementation of SelectableItemViewBase. Contains a start icon, title, description, and
 * optional end icon (GONE by default). Views may be accessed through protected member variables.
 *
 * @param <E> The type of the item associated with this SelectableItemViewBase.
 */
public abstract class SelectableItemView<E> extends CheckableSelectableItemView<E> {
    /** The LinearLayout containing the rest of the views for the selectable item. */
    protected LinearLayout mContentView;

    /** An icon displayed at the start of the item row. */
    protected ImageView mStartIconView;

    /** An optional button displayed at the end of the item row, GONE by default. */
    protected AppCompatImageButton mEndButtonView;

    /** A title line displayed between the start and (optional) end icon. */
    protected TextView mTitleView;

    /** A description line displayed below the title line. */
    protected TextView mDescriptionView;

    /** Layout res to be used when inflating the view, used to swap in the visual refresh. */
    private int mLayoutRes;

    /** Levels for the background. */
    private final int mDefaultLevel;

    private final int mSelectedLevel;

    /** The resource for the start icon background. */
    private int mStartIconBackgroundRes;

    /** Constructor for inflating from XML. */
    public SelectableItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mLayoutRes = R.layout.modern_list_item_view;
        mStartIconBackgroundRes = R.drawable.list_item_icon_modern_bg;
        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.list_item_level_selected);
    }

    // CheckableSelectableItemView implementation.

    @Override
    protected ImageView getIconView() {
        return mStartIconView;
    }

    @Override
    protected @Nullable ColorStateList getDefaultIconTint() {
        return null;
    }

    @Override
    protected int getSelectedLevel() {
        return mSelectedLevel;
    }

    @Override
    protected int getDefaultLevel() {
        return mDefaultLevel;
    }

    // FrameLayout implementations.

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        inflateAndPopulateViewVariables();
    }

    private void inflateAndPopulateViewVariables() {
        LayoutInflater.from(getContext()).inflate(mLayoutRes, this);

        mContentView = findViewById(R.id.content);
        mStartIconView = findViewById(R.id.start_icon);
        mEndButtonView = findViewById(R.id.end_button);
        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);

        if (mStartIconView != null) {
            mStartIconView.setBackgroundResource(mStartIconBackgroundRes);
            ImageViewCompat.setImageTintList(mStartIconView, getDefaultIconTint());
        }
    }

    /**
     * Set drawable for the start icon view. Note that you may need to use this method instead of
     * mIconView#setImageDrawable to ensure icon view is correctly set in selection mode.
     */
    protected void setStartIconDrawable(Drawable iconDrawable) {
        setIconDrawable(iconDrawable);
    }

    /** Returns the drawable set for the start icon view, if any. */
    protected Drawable getStartIconDrawable() {
        return getIconDrawable();
    }
}
