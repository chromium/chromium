// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemProcessor;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the suggestion headers. */
public class HeaderProcessor implements DropdownItemProcessor {
    private final int mMinimumHeight;

    /**
     * @param context An Android context.
     */
    public HeaderProcessor(@NonNull Context context) {
        mMinimumHeight =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.HEADER;
    }

    @Override
    public int getMinimumViewHeight() {
        return mMinimumHeight;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(HeaderViewProperties.ALL_KEYS);
    }

    /**
     * Populate a model for the group header.
     *
     * @param model The model to populate.
     * @param headerText Text to be displayed for this group header.
     */
    public void populateModel(@NonNull PropertyModel model, @NonNull String headerText) {
        model.set(HeaderViewProperties.TITLE, headerText);
    }
}
