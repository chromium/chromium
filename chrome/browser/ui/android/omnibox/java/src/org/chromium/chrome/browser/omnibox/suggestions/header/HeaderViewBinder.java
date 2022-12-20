// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.res.Resources;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the header suggestion view. */
public class HeaderViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, HeaderView view, PropertyKey propertyKey) {
        if (HeaderViewProperties.TITLE == propertyKey) {
            view.setText(model.get(HeaderViewProperties.TITLE));
        } else if (propertyKey == SuggestionCommonProperties.COLOR_SCHEME) {
            final boolean isIncognito = model.get(SuggestionCommonProperties.COLOR_SCHEME)
                    == BrandedColorScheme.INCOGNITO;
            view.setTextAppearance(ChromeColors.getTextMediumThickSecondaryStyle(isIncognito));
        } else if (propertyKey == SuggestionCommonProperties.LAYOUT_DIRECTION) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (propertyKey == HeaderViewProperties.USE_MODERNIZED_HEADER_PADDING) {
            boolean useModernizedHeaderPadding =
                    model.get(HeaderViewProperties.USE_MODERNIZED_HEADER_PADDING);
            Resources res = view.getResources();

            int minHeight = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);
            int paddingStart =
                    res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_start);
            int paddingTop =
                    res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_top);
            int paddingBottom =
                    res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_bottom);

            // Use modified padding if the phase 2 feature is enabled.
            if (useModernizedHeaderPadding) {
                minHeight = res.getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_header_height_modern_phase2);
                paddingStart += res.getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing);
                // TODO(crbug.com/1372596): Header view is off center and we should fix this.
                paddingBottom = 0;
            }

            view.setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
        }
    }
}
