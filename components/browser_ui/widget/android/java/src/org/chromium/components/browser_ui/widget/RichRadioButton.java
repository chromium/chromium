// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Checkable;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ChromeImageView;

/**
 * A custom view that combines a RadioButton with an optional icon, title, and optional description.
 * It implements the {@link Checkable} interface to behave like a standard radio button and provides
 * methods to customize its content and layout.
 */
@NullMarked
public class RichRadioButton extends LinearLayout implements Checkable {

    private FrameLayout mIconContainer;
    private ChromeImageView mItemIcon;
    private TextView mItemTitle;
    private TextView mItemDescription;
    private RadioButton mItemRadioButton;

    private boolean mIsChecked;

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

        mIconContainer = findViewById(R.id.rich_radio_button_icon_container);
        mItemIcon = findViewById(R.id.rich_radio_button_icon);
        mItemTitle = findViewById(R.id.rich_radio_button_title);
        mItemDescription = findViewById(R.id.rich_radio_button_description);
        mItemRadioButton = findViewById(R.id.rich_radio_button_radio_button);

        setOnClickListener(v -> toggle());

        ViewCompat.setAccessibilityDelegate(
                this,
                new AccessibilityDelegateCompat() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfoCompat info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setCheckable(true);
                        info.setChecked(mIsChecked);
                        info.setClassName(RadioButton.class.getName());
                    }
                });
    }

    /**
     * Binds data to the item view.
     *
     * @param iconResId Optional drawable resource ID for the icon. Pass 0 to hide the icon.
     * @param title The title text for the item.
     * @param description Optional description text. Pass null or empty string to hide.
     */
    public void setItemData(
            @DrawableRes int iconResId,
            @NonNull String title,
            @Nullable String description,
            boolean isVertical) {
        if (iconResId != 0) {
            mItemIcon.setImageResource(iconResId);
            mItemIcon.setVisibility(VISIBLE);
            mIconContainer.setVisibility(VISIBLE);
        } else {
            mItemIcon.setVisibility(GONE);
            mIconContainer.setVisibility(GONE);
        }
        mItemTitle.setText(title);
        if (description != null && !description.isEmpty()) {
            mItemDescription.setText(description);
            mItemDescription.setVisibility(VISIBLE);
        } else {
            mItemDescription.setVisibility(GONE);
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
