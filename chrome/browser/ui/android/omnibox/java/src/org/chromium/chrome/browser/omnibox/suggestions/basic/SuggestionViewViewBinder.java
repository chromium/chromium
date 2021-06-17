// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
public class SuggestionViewViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            TextView tv = view.findViewById(R.id.line_1);
            tv.setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (propertyKey == SuggestionCommonProperties.OMNIBOX_THEME) {
            updateSuggestionTextColor(view, model);
        } else if (propertyKey == SuggestionViewProperties.IS_SEARCH_SUGGESTION) {
            updateSuggestionTextColor(view, model);
            // https://crbug.com/609680: ensure URLs are always composed LTR and that their
            // components are not re-ordered.
            final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
            final TextView tv = view.findViewById(R.id.line_2);
            tv.setTextDirection(
                    isSearch ? TextView.TEXT_DIRECTION_INHERIT : TextView.TEXT_DIRECTION_LTR);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_TEXT) {
            TextView tv = view.findViewById(R.id.line_2);
            final SuggestionSpannable span = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
            if (!TextUtils.isEmpty(span)) {
                tv.setText(span);
                tv.setVisibility(View.VISIBLE);
            } else {
                tv.setVisibility(View.GONE);
            }
        } else if (propertyKey == SuggestionViewProperties.ALLOW_WRAP_AROUND) {
            final boolean allowWrapAround = model.get(SuggestionViewProperties.ALLOW_WRAP_AROUND);
            TextView tv = view.findViewById(R.id.line_1);
            tv.setMaxLines(allowWrapAround ? 2 : 1);
        }
    }

    private static void updateSuggestionTextColor(View view, PropertyModel model) {
        final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        final boolean useDarkMode = !OmniboxResourceProvider.isDarkMode(
                model.get(SuggestionCommonProperties.OMNIBOX_THEME));
        final TextView line1 = view.findViewById(R.id.line_1);
        final TextView line2 = view.findViewById(R.id.line_2);

        @ColorRes
        final int color1 =
                useDarkMode ? R.color.default_text_color_dark : R.color.default_text_color_light;
        line1.setTextColor(ApiCompatibilityUtils.getColor(view.getResources(), color1));

        @ColorRes
        final int color2 = isSearch ? (useDarkMode ? R.color.default_text_color_secondary_dark
                                                   : R.color.default_text_color_secondary_light)
                                    : (useDarkMode ? R.color.suggestion_url_dark_modern
                                                   : R.color.suggestion_url_light_modern);
        line2.setTextColor(ApiCompatibilityUtils.getColor(view.getResources(), color2));
    }
}
