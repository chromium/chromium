// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import android.graphics.drawable.Drawable;

import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder proxy for divider line in Suggestions. */
public class DividerLineViewBinder {
    public static void bind(PropertyModel model, DividerLineView view, PropertyKey propertyKey) {
        if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            Drawable drawable = OmniboxResourceProvider.resolveAttributeToDrawable(
                    view.getContext(), model.get(SuggestionCommonProperties.COLOR_SCHEME),
                    android.R.attr.listDivider);
            view.getDivider().setBackground(drawable);
        }
    }
}
