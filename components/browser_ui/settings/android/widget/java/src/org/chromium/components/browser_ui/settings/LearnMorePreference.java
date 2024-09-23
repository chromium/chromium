// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * A preference with a clickable "Learn more" link.
 *
 * <p>During {@link androidx.preference.PreferenceFragmentCompat#onCreatePreferences}, call {@link
 * #setOnPreferenceClickListener} to configure the link click behavior.
 */
public class LearnMorePreference extends Preference {
    public LearnMorePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setTitle(R.string.learn_more);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setTextAppearance(R.style.TextAppearance_TextLarge_Link);
        titleView.setClickable(true);
        titleView.setOnClickListener(v -> getOnPreferenceClickListener().onPreferenceClick(this));
    }
}
