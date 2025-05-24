// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A preference with a highlighted background and prominent call to action button. */
@NullMarked
public class CardWithButtonPreference extends ChromeBasePreference {
    private @Nullable CharSequence mButtonText;
    private @Nullable Runnable mOnButtonClick;

    /**
     * Constructor for CardWithButtonPreference.
     *
     * @param context The context of the preference.
     * @param attrs The attributes of the preference.
     */
    public CardWithButtonPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.card_with_button_preference_layout);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Only the button is clickable (https://crbug.com/382089385).
        holder.itemView.setClickable(false);

        Button button = (Button) holder.findViewById(R.id.card_button);
        button.setText(mButtonText);
        button.setOnClickListener(
                (v) -> {
                    if (mOnButtonClick != null) {
                        mOnButtonClick.run();
                    }
                });
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

    /**
     * Sets the button on click runnable.
     *
     * @param onButtonClick The runnable to be invoked when the button is clicked.
     */
    public void setOnButtonClick(Runnable onButtonClick) {
        mOnButtonClick = onButtonClick;
    }
}
