// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * A {@link Preference} that provides button functionality.
 *
 * Preference.getOnPreferenceClickListener().onPreferenceClick() is called when the button is
 * clicked.
 */
public class ButtonPreference extends Preference {
    /**
     * Constructor for inflating from XML
     */
    public ButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.button_preference_layout);
        setWidgetLayoutResource(R.layout.button_preference_button);

        // Only the inner button element should be focusable.
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        Button button = (Button) holder.findViewById(R.id.button_preference);
        button.setText(getTitle());
        button.setOnClickListener(v -> {
            if (getOnPreferenceClickListener() != null) {
                getOnPreferenceClickListener().onPreferenceClick(ButtonPreference.this);
            }
        });
    }
}
