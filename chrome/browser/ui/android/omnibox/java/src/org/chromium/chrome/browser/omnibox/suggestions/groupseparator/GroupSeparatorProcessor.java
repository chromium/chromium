// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.groupseparator;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that handles model and view creation for the suggestion group separator.
 * TODO(crbug.com/41491951): Move functionality to HeaderView and remove this component.
 */
public class GroupSeparatorProcessor implements DropdownItemProcessor {
    private final int mMinimumHeight;

    /**
     * @param context An Android context.
     */
    public GroupSeparatorProcessor(Context context) {
        mMinimumHeight =
                context.getResources().getDimensionPixelSize(R.dimen.divider_height)
                        + context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.omnibox_suggestion_list_divider_line_padding);
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.GROUP_SEPARATOR;
    }

    @Override
    public int getMinimumViewHeight() {
        return mMinimumHeight;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(SuggestionCommonProperties.ALL_KEYS);
    }
}
