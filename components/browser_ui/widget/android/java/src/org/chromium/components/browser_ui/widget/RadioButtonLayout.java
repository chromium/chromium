// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import androidx.annotation.Nullable;

import java.util.List;

/**
 * Manages a group of exclusive RadioButtons.
 *
 * -------------------------------------------------
 * | O | MESSAGE #1                                |
 * | O | MESSAGE #N                                |
 * -------------------------------------------------
 */
public final class RadioButtonLayout extends RadioGroup {
    public static final int INVALID_INDEX = -1;

    public RadioButtonLayout(Context context) {
        this(context, null);
    }

    public RadioButtonLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Adds a set of standard radio buttons for the given messages and adds them to the layout.
     *
     * @param messages      Messages to display for the options.
     * @param tags          Optional list of tags to attach to the buttons.
     */
    public void addOptions(List<CharSequence> messages, @Nullable List<?> tags) {
        if (tags != null) assert tags.size() == messages.size();

        for (int i = 0; i < messages.size(); i++) {
            RadioButton button =
                    (RadioButton)
                            LayoutInflater.from(getContext())
                                    .inflate(R.layout.radio_button_layout_element, null);
            button.setText(messages.get(i));
            if (tags != null) button.setTag(tags.get(i));

            addView(button, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }
    }

    /**
     * Marks a RadioButton child as being checked.
     *
     * Android doesn't provide a way of generating View IDs on the fly before API level 17, so this
     * function requires passing in the child's index.  Passing in {@link #INVALID_INDEX} marks them
     * all as de-selected.
     *
     * @param childIndex Index of the child to select.
     */
    public void selectChildAtIndex(int childIndex) {
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            RadioButton child = (RadioButton) getChildAt(i);
            child.setChecked(i == childIndex);
        }
    }
}
