// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;

import androidx.preference.PreferenceViewHolder;

/**
 * A preference with a highlighted background and prominent call to action button.
 *
 * <p>Preference.getOnPreferenceClickListener().onPreferenceClick() is called when the button is
 * clicked.
 */
public class CardWithButtonPreference extends ChromeBasePreference implements View.OnClickListener {
    private CharSequence mButtonText;

    /**
     * Constructor for CardWithButtonPreference.
     *
     * @param context The context of the preference.
     * @param attrs The attributes of the preference.
     */
    public CardWithButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.card_with_button_preference_layout);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        Button button = (Button) holder.findViewById(R.id.card_button);
        button.setText(mButtonText);
        button.setOnClickListener(this);
    }

    /**
     * Sets the text of the call to action button.
     *
     * @param buttonText The button text.
     */
    public void setButtonText(CharSequence buttonText) {
        mButtonText = buttonText;
        notifyChanged();
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        if (getOnPreferenceClickListener() != null) {
            getOnPreferenceClickListener().onPreferenceClick(CardWithButtonPreference.this);
        }
    }
}
