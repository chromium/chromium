// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import android.content.Context;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the suggestion divider line. */
public class DividerLineProcessor implements DropdownItemProcessor {
    private final int mMinimumHeight;

    /**
     * @param context An Android context.
     */
    public DividerLineProcessor(Context context) {
        mMinimumHeight = context.getResources().getDimensionPixelSize(R.dimen.divider_height)
                + context.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_list_divider_line_padding);
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.DIVIDER_LINE;
    }

    @Override
    public int getMinimumViewHeight() {
        return mMinimumHeight;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionCommonProperties.ALL_KEYS);
    }
}
