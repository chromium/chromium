// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
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
import androidx.annotation.StringRes;

import com.google.android.material.materialswitch.MaterialSwitch;

/**
 * Chrome workaround for material switch wrapped within a layout to scale down the size of the
 * switch, while maintaining the size of the text.
 *
 * <p>Note that this class has many limitations, so features that'd like to further customize the
 * behavior of the switch and text are highly recommended to implement their own TextView +
 * MaterialSwitch in the layout.
 *
 * <p>The limitations including but not limited to:
 *
 * <ul>
 *   Resource attribute defined in xml for {@link MaterialSwitch} are not forwarded to the wrapped
 *   switch (e.g. android:textAppearance)
 * </ul>
 *
 * <ul>
 *   The ability to perform long-press & drag to flip the switch is disabled, due to having the
 *   LinearLayout to be focusable to have a consistent behavior with MaterialSwitch in talkback.
 *   This behavior makes it similar to the behavior as a SwitchPreferenceCompat.
 * </ul>
 */
public class MaterialSwitchWithText extends LinearLayout implements Checkable, OnClickListener {
    private final MaterialSwitch mSwitch;
    private final TextView mTextView;

    public MaterialSwitchWithText(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public MaterialSwitchWithText(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        LayoutInflater.from(context).inflate(R.layout.material_switch_with_text, this);

        setOrientation(LinearLayout.HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setMinimumHeight(getResources().getDimensionPixelSize(R.dimen.switch_with_text_min_height));
        setFocusable(true);

        mTextView = findViewById(R.id.switch_text);
        mSwitch = findViewById(R.id.switch_widget);

        setOnClickListener(this);

        TypedArray textStyles =
                getContext().obtainStyledAttributes(attrs, new int[] {android.R.attr.text});
        @StringRes int textId = textStyles.getResourceId(0, 0);
        if (textId != 0) mTextView.setText(textId);
        textStyles.recycle();
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
