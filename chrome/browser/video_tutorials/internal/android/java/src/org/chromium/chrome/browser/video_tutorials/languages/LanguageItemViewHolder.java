// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view holder for individual list items.
 */
class LanguageItemViewHolder {
    /** Builder method to create the language item view. */
    public static View buildView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.language_card, parent, false);
    }

    /** Binder method to bind the list view with the model properties. */
    public static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        final RadioButtonWithDescription radioButton =
                view.findViewById(R.id.language_radio_button);
        if (propertyKey == LanguageItemProperties.NAME) {
            radioButton.setPrimaryText(model.get(LanguageItemProperties.NAME));
        } else if (propertyKey == LanguageItemProperties.NATIVE_NAME) {
            radioButton.setDescriptionText(model.get(LanguageItemProperties.NATIVE_NAME));
        } else if (propertyKey == LanguageItemProperties.IS_SELECTED) {
            radioButton.setChecked(model.get(LanguageItemProperties.IS_SELECTED));
        } else if (propertyKey == LanguageItemProperties.SELECTION_CALLBACK) {
            radioButton.setOnCheckedChangeListener(checkedRadioButton -> {
                if (!radioButton.isChecked()) return;
                model.get(LanguageItemProperties.SELECTION_CALLBACK)
                        .onResult(model.get(LanguageItemProperties.LOCALE));
            });
        }
    }
}
