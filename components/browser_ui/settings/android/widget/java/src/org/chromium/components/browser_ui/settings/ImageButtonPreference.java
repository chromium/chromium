// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.preference.PreferenceViewHolder;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * A preference with an ImageButton as widget. Clicks on the image button will trigger the
 * OnPreferenceClickListener. Clicks on the preference itself are ignored.
 */
public class ImageButtonPreference extends ChromeBasePreference implements View.OnClickListener {
    private @DrawableRes int mImage;
    private String mContentDescription;

    public ImageButtonPreference(Context context) {
        super(context);
        initialize();
    }

    public ImageButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        initialize();
    }

    /** Set the image and content description for this preference. */
    public void setImage(@DrawableRes int image, String contentDescription) {
        mImage = image;
        mContentDescription = contentDescription;
    }

    private void initialize() {
        setSelectable(false);
        setWidgetLayoutResource(R.layout.image_button_widget);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ChromeImageButton imageButton = (ChromeImageButton) holder.findViewById(R.id.image_button);
        imageButton.setImageResource(mImage);
        if (mContentDescription != null) {
            imageButton.setContentDescription(mContentDescription);
        }
        imageButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        if (getOnPreferenceClickListener() != null) {
            getOnPreferenceClickListener().onPreferenceClick(this);
        }
    }
}
