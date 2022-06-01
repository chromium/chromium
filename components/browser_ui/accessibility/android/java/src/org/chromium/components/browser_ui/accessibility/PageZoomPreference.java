// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * Custom preference for the page zoom section of Accessibility Settings.
 */
public class PageZoomPreference extends Preference implements SeekBar.OnSeekBarChangeListener {
    private int mInitialValue;

    public PageZoomPreference(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.page_zoom_preference);
    }

    @Override
    public void onBindViewHolder(@NonNull PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Re-use the main control layout, but remove extra padding and background.
        LinearLayout container = (LinearLayout) holder.findViewById(R.id.page_zoom_view_container);
        int top = container.getPaddingTop();
        int bot = container.getPaddingBottom();
        container.setBackground(null);
        container.setPadding(0, top, 0, bot);

        TextView mCurrentValueText =
                (TextView) holder.findViewById(R.id.page_zoom_current_value_text);
        mCurrentValueText.setText(
                getContext().getResources().getString(R.string.page_zoom_factor, 100));

        SeekBar mCurrentValueSlider = (SeekBar) holder.findViewById(R.id.page_zoom_slider);
        mCurrentValueSlider.setProgress(mInitialValue);
        mCurrentValueSlider.setOnSeekBarChangeListener(this);
    }

    /**
     * Initial value to set the progress of the seekbar.
     * @param value int - existing user pref value (or default).
     */
    public void setInitialValue(int value) {
        mInitialValue = value;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {}

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        // When a user stops changing the slider value, record the new value in prefs.
        callChangeListener(seekBar.getProgress());
    }
}