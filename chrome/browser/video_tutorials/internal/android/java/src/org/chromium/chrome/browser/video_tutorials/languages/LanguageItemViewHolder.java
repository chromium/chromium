// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.video_tutorials.R;
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
        if (propertyKey == LanguageItemProperties.NAME) {
            TextView title = view.findViewById(R.id.title);
            title.setText(model.get(LanguageItemProperties.NAME));
        } else if (propertyKey == LanguageItemProperties.NATIVE_NAME) {
            TextView description = view.findViewById(R.id.description);
            description.setText(model.get(LanguageItemProperties.NATIVE_NAME));
        } else if (propertyKey == LanguageItemProperties.IS_SELECTED) {
            ImageView imageView = view.findViewById(R.id.check);
            imageView.setSelected(model.get(LanguageItemProperties.IS_SELECTED));
        } else if (propertyKey == LanguageItemProperties.SELECTION_CALLBACK) {
            view.setOnClickListener(v -> {
                model.get(LanguageItemProperties.SELECTION_CALLBACK)
                        .onResult(model.get(LanguageItemProperties.LOCALE));
            });
        }
    }
}
