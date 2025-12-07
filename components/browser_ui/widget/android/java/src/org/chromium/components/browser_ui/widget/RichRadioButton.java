// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Checkable;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ChromeImageView;

/**
 * A custom view that combines a RadioButton with an optional icon, title, and optional description.
 * It implements the {@link Checkable} interface to behave like a standard radio button and provides
 * methods to customize its content and layout. It can display its content in a horizontal or
 * vertical orientation.
 */
@NullMarked
public class RichRadioButton extends ConstraintLayout implements Checkable {

    private FrameLayout mIconContainer;
    private ChromeImageView mItemIcon;
    private TextView mItemTitle;
    private TextView mItemDescription;
    private RadioButton mItemRadioButton;

    private ConstraintSet mHorizontalConstraints;
    private ConstraintSet mVerticalConstraints;

    private ConstraintLayout mRootItemLayout;

    private boolean mIsChecked;
    private boolean mIsVerticalLayout;

    private int mDefaultRootLayoutPaddingStart;
    private int mDefaultRootLayoutPaddingTop;
    private int mDefaultRootLayoutPaddingEnd;
    private int mDefaultRootLayoutPaddingBottom;

    private ViewGroup.MarginLayoutParams mDefaultTitleLayoutParams;
    private ViewGroup.MarginLayoutParams mDefaultRadioButtonLayoutParams;

    public RichRadioButton(@NonNull Context context) {
        super(context);
        init(context);
    }

    public RichRadioButton(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public RichRadioButton(
            @NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    private void init(@NonNull Context context) {
        LayoutInflater.from(context).inflate(R.layout.rich_radio_button, this, true);

        mHorizontalConstraints = new ConstraintSet();
        mHorizontalConstraints.load(context, R.xml.rich_radio_button_horizontal_constraints);
        mVerticalConstraints = new ConstraintSet();
        mVerticalConstraints.load(context, R.xml.rich_radio_button_vertical_constraints);

        mRootItemLayout = findViewById(R.id.root_item_layout);
        mIconContainer = findViewById(R.id.rich_radio_button_icon_container);
        mItemIcon = findViewById(R.id.rich_radio_button_icon);
        mItemTitle = findViewById(R.id.rich_radio_button_title);
        mItemDescription = findViewById(R.id.rich_radio_button_description);
        mItemRadioButton = findViewById(R.id.rich_radio_button_radio_button);

        setOnClickListener(v -> toggle());

        mDefaultRootLayoutPaddingStart = mRootItemLayout.getPaddingStart();
        mDefaultRootLayoutPaddingTop = mRootItemLayout.getPaddingTop();
        mDefaultRootLayoutPaddingEnd = mRootItemLayout.getPaddingEnd();
        mDefaultRootLayoutPaddingBottom = mRootItemLayout.getPaddingBottom();

        mDefaultTitleLayoutParams =
                new LinearLayout.LayoutParams(
                        (LinearLayout.LayoutParams) mItemTitle.getLayoutParams());

        mDefaultRadioButtonLayoutParams =
                new ConstraintLayout.LayoutParams(
                        (ConstraintLayout.LayoutParams) mItemRadioButton.getLayoutParams());
    }

    /**
     * Binds data to the item view.
     *
     * @param iconResId Optional drawable resource ID for the icon. Pass 0 to hide the icon.
     * @param title The title text for the item.
     * @param description Optional description text. Pass null or empty string to hide.
     * @param isInternalVertical True if the item's internal content should be vertically stacked,
     *     false for horizontal.
     */
    public void setItemData(
            @DrawableRes int iconResId,
            @NonNull String title,
            @Nullable String description,
            boolean isInternalVertical) {
        if (iconResId != 0) {
            mItemIcon.setImageResource(iconResId);
            mItemIcon.setVisibility(VISIBLE);
            mIconContainer.setVisibility(VISIBLE);
            resetLayoutWithIcon();
        } else {
            mItemIcon.setVisibility(GONE);
            mIconContainer.setVisibility(GONE);
            adjustLayoutWithoutIcon();
        }
        mItemTitle.setText(title);
        if (description != null && !description.isEmpty()) {
            mItemDescription.setText(description);
            mItemDescription.setVisibility(VISIBLE);
        } else {
            mItemDescription.setVisibility(GONE);
        }
        setOrientation(isInternalVertical);
    }

    private void adjustLayoutWithoutIcon() {
        int layoutVerticalPaddingPx =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.rich_radio_button_without_icon_vertical_padding);
        mRootItemLayout.setPaddingRelative(
                0,
                layoutVerticalPaddingPx,
                mRootItemLayout.getPaddingEnd(),
                layoutVerticalPaddingPx);

        int titleMarginPx =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.rich_radio_button_title_margin);

        LinearLayout.LayoutParams currentTitleParams =
                (LinearLayout.LayoutParams) mItemTitle.getLayoutParams();
        LinearLayout.LayoutParams newTitleParams =
                new LinearLayout.LayoutParams(currentTitleParams);
        newTitleParams.setMargins(titleMarginPx, titleMarginPx, 0, titleMarginPx);
        mItemTitle.setLayoutParams(newTitleParams);
        int radioButtonMarginPx =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.rich_radio_button_radio_button_margin_end);
        ConstraintLayout.LayoutParams currentRadioButtonParams =
                (ConstraintLayout.LayoutParams) mItemRadioButton.getLayoutParams();
        ConstraintLayout.LayoutParams newRadioButtonParams =
                new ConstraintLayout.LayoutParams(currentRadioButtonParams);
        newRadioButtonParams.setMarginEnd(radioButtonMarginPx);
        mItemRadioButton.setLayoutParams(newRadioButtonParams);
    }

    private void resetLayoutWithIcon() {
        mRootItemLayout.setPaddingRelative(
                mDefaultRootLayoutPaddingStart,
                mDefaultRootLayoutPaddingTop,
                mDefaultRootLayoutPaddingEnd,
                mDefaultRootLayoutPaddingBottom);

        mItemTitle.setLayoutParams(mDefaultTitleLayoutParams);
        mItemRadioButton.setLayoutParams(mDefaultRadioButtonLayoutParams);
    }

    /**
     * Sets the orientation of the item's internal layout.
     *
     * @param isVertical True for vertical stacking, false for horizontal.
     */
    private void setOrientation(boolean isVertical) {
        if (mIsVerticalLayout != isVertical) {
            mIsVerticalLayout = isVertical;

            if (isVertical) {
                mVerticalConstraints.applyTo(mRootItemLayout);
                mItemTitle.setGravity(android.view.Gravity.CENTER_HORIZONTAL);
                mItemDescription.setGravity(android.view.Gravity.CENTER_HORIZONTAL);
                mItemDescription.setMaxLines(1);
            } else {
                mHorizontalConstraints.applyTo(mRootItemLayout);
                mItemTitle.setGravity(Gravity.NO_GRAVITY);
                mItemDescription.setGravity(Gravity.NO_GRAVITY);
            }
        }
    }

    @Override
    public void setChecked(boolean checked) {
        if (mIsChecked != checked) {
            mIsChecked = checked;
            mItemRadioButton.setChecked(checked);
            refreshDrawableState();
        }
    }

    @Override
    public boolean isChecked() {
        return mIsChecked;
    }

    @Override
    public void toggle() {
        setChecked(!mIsChecked);
    }

    private static final int[] CHECKED_STATE_SET = {android.R.attr.state_checked};

    @Override
    public int[] onCreateDrawableState(int extraForSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraForSpace + 1);
        if (isChecked()) {
            mergeDrawableStates(drawableState, CHECKED_STATE_SET);
        }
        return drawableState;
    }
}
