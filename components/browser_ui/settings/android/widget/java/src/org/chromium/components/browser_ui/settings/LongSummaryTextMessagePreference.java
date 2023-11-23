// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

/** A custom version of a TextMessagePreference that allows for very long summary texts. */
public class LongSummaryTextMessagePreference extends TextMessagePreference {
    /** Constructor for inflating from XML. */
    public LongSummaryTextMessagePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.long_summary_text_message_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView summaryView = (TextView) holder.findViewById(android.R.id.summary);
        summaryView.setMaxLines(100);
    }
}
