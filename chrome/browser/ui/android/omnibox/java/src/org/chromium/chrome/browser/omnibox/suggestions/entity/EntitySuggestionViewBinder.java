// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding EntitySuggestion properties to its view. */
public class EntitySuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (EntitySuggestionViewProperties.SUBJECT_TEXT == propertyKey) {
            final TextView tv = view.findViewById(R.id.entity_subject);
            tv.setText(model.get(EntitySuggestionViewProperties.SUBJECT_TEXT));
        } else if (EntitySuggestionViewProperties.DESCRIPTION_TEXT == propertyKey) {
            final TextView tv = view.findViewById(R.id.entity_description);
            final String text = model.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT);
            if (TextUtils.isEmpty(text)) {
                tv.setVisibility(View.GONE);
            } else {
                tv.setVisibility(View.VISIBLE);
                tv.setText(model.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT));
            }
        }
    }
}
