// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.text.method.MovementMethod;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

/** A preference that displays informational text, and a summary which can contain a link. */
public class TextMessagePreference extends ChromeBasePreference {
    private TextView mSummaryView;
    private MovementMethod mMovementMethod = LinkMovementMethod.getInstance();

    /** Constructor for inflating from XML. */
    public TextMessagePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mSummaryView = (TextView) holder.findViewById(android.R.id.summary);
        setSummaryMovementMethod(mMovementMethod);
    }

    /** @param movementMethod Set the movement method of the summary TextView. */
    public void setSummaryMovementMethod(MovementMethod movementMethod) {
        mMovementMethod = movementMethod;
        if (mSummaryView != null) {
            mSummaryView.setMovementMethod(movementMethod);
        }
    }
}
