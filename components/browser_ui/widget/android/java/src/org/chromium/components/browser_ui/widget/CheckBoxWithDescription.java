// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;

/**
 * A CheckBox with a primary and descriptive text to the right.
 * The object will be inflated from {@link R.layout.checkbox_with_description).
 * TODO(crbug.com/40862238): Add CompoundButtonWithDescription to avoid duplicate code with
 * RadioButtonWithDescription.
 */
public class CheckBoxWithDescription extends ConstraintLayout implements OnClickListener {
    private CheckBox mCheckBox;
    private TextView mPrimary;
    private TextView mDescription;

    /** Constructor for inflating via XML. */
    public CheckBoxWithDescription(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.checkbox_with_description, this, true);
        setViewsInternal();

        // We want CheckBoxWithDescription to handle the clicks itself.
        setOnClickListener(this);
        // Make it focusable for navigation via key events (tab/up/down keys).
        setFocusable(true);
    }

    /** Set the view elements that included in xml internally. */
    private void setViewsInternal() {
        mCheckBox = getCheckBoxView();
        mPrimary = getPrimaryTextView();
        mDescription = getDescriptionTextView();
    }

    /** @return CheckBox View inside this {@link CheckBoxWithDescription}. */
    private CheckBox getCheckBoxView() {
        return (CheckBox) findViewById(R.id.checkbox);
    }

    /** @return TextView displayed as primary inside this {@link CheckBoxWithDescription}. */
    @VisibleForTesting
    TextView getPrimaryTextView() {
        return (TextView) findViewById(R.id.primary);
    }

    /** @return TextView displayed as description inside this {@link CheckBoxWithDescription}. */
    @VisibleForTesting
    TextView getDescriptionTextView() {
        return (TextView) findViewById(R.id.description);
    }

    @Override
    public void onClick(View v) {
        setChecked(!isChecked());
    }

    /** Sets the text shown in the primary section. */
    public void setPrimaryText(CharSequence text) {
        mPrimary.setText(text);
    }

    /** @return The text shown in the primary section. */
    @VisibleForTesting
    CharSequence getPrimaryText() {
        return mPrimary.getText();
    }

    /** Sets the text shown in the description section. */
    public void setDescriptionText(CharSequence text) {
        mDescription.setText(text);

        if (TextUtils.isEmpty(text)) {
            mDescription.setVisibility(View.GONE);
        } else {
            mDescription.setVisibility(View.VISIBLE);
        }
    }

    /** @return The text shown in the description section. */
    @VisibleForTesting
    CharSequence getDescriptionText() {
        return mDescription.getText();
    }

    /** Returns true if checked. */
    public boolean isChecked() {
        return mCheckBox.isChecked();
    }

    /**
     * Sets the checked status of this checkbox.
     * @param checked Whether this checkbox will be checked.
     */
    public void setChecked(boolean checked) {
        mCheckBox.setChecked(checked);
    }
}
