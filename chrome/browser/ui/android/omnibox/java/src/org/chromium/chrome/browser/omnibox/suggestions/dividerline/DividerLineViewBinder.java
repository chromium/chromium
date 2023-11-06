// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder proxy for divider line in Suggestions. */
public class DividerLineViewBinder {
    public static void bind(PropertyModel model, DividerLineView view, PropertyKey propertyKey) {
        if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            if (model.get(SuggestionCommonProperties.COLOR_SCHEME)
                    == BrandedColorScheme.INCOGNITO) {
                view.getDivider().setBackgroundResource(R.color.divider_line_bg_color_light);
            } else {
                view.getDivider()
                        .setBackgroundColor(
                                SemanticColorUtils.getDividerLineBgColor(view.getContext()));
            }
        }
    }
}
