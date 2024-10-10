// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Checkable;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.google.android.material.materialswitch.MaterialSwitch;

// TODO(crbug.com/326040498): Merge this class with MaterialSwitchWithText (this class can just hide
// the summary if it is empty and will then be identical to MaterialSwitchWithText).

/**
 * MaterialSwitchWithTitleAndSummary is a custom Android view that combines a MaterialSwitch with a
 * title and summary text. It provides a convenient way to display a switch with associated labels
 * in your UI.
 */
public class MaterialSwitchWithTitleAndSummary extends LinearLayout
        implements Checkable, OnClickListener {
    public interface Listener {
        /** Called when the user flips the switch. */
        void onSwitchtoggled();
    }

    private final MaterialSwitch mSwitch;
    private final TextView mTitleTextView;
    private final TextView mSummaryTextView;

    public MaterialSwitchWithTitleAndSummary(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public MaterialSwitchWithTitleAndSummary(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        LayoutInflater.from(context).inflate(R.layout.material_switch_with_title_and_summary, this);

        setOrientation(LinearLayout.HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setMinimumHeight(getResources().getDimensionPixelSize(R.dimen.switch_with_text_min_height));
        setFocusable(true);

        mTitleTextView = findViewById(R.id.titleText);
        mSummaryTextView = findViewById(R.id.summaryText);
        mSwitch = findViewById(R.id.switch_widget);
        mSwitch.setChecked(true);

        setOnClickListener(this);
    }

    public void setTitleText(String text) {
        mTitleTextView.setText(text);
    }

    public void setSummaryText(String text) {
        mSummaryTextView.setText(text);
    }

    @Override
    public void setChecked(boolean checked) {
        mSwitch.setChecked(checked);
    }

    @Override
    public boolean isChecked() {
        return mSwitch.isChecked();
    }

    @Override
    public void toggle() {
        mSwitch.toggle();
    }

    @Override
    public void onClick(View view) {
        toggle();
    }

    /**
     * Set the OnCheckedChangeListener for the switch.
     *
     * @see CompoundButton#setOnCheckedChangeListener(CompoundButton.OnCheckedChangeListener).
     */
    public void setOnCheckedChangeListener(CompoundButton.OnCheckedChangeListener listener) {
        mSwitch.setOnCheckedChangeListener(listener);
    }
}
