// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
public class SuggestionViewViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            TextView tv = view.findViewById(R.id.line_1);
            tv.setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT));
            int minHeight =
                    tv.getResources()
                            .getDimensionPixelSize(
                                    tv.getLineCount() > 1
                                            ? R.dimen
                                                    .omnibox_suggestion_minimum_content_height_multiline
                                            : R.dimen.omnibox_suggestion_minimum_content_height);
            view.setMinimumHeight(minHeight);
        } else if (propertyKey == SuggestionCommonProperties.COLOR_SCHEME) {
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
            int maxLines = allowWrapAround ? 2 : 1;
            if (tv.getMaxLines() != maxLines) {
                tv.setMaxLines(maxLines);
            }
        }
    }

    private static void updateSuggestionTextColor(View view, PropertyModel model) {
        final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        final @BrandedColorScheme int brandedColorScheme =
                model.get(SuggestionCommonProperties.COLOR_SCHEME);
        final TextView line1 = view.findViewById(R.id.line_1);
        final TextView line2 = view.findViewById(R.id.line_2);

        final Context context = view.getContext();
        final @ColorInt int color1 =
                OmniboxResourceProvider.getSuggestionPrimaryTextColor(context, brandedColorScheme);
        line1.setTextColor(color1);

        final @ColorInt int color2 =
                isSearch
                        ? OmniboxResourceProvider.getSuggestionSecondaryTextColor(
                                context, brandedColorScheme)
                        : OmniboxResourceProvider.getSuggestionUrlTextColor(
                                context, brandedColorScheme);
        line2.setTextColor(color2);
    }
}
