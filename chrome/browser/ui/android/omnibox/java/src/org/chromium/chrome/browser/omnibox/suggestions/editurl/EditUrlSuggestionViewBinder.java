// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Binder proxy for EditURL Suggestions. */
public class EditUrlSuggestionViewBinder
        implements ViewBinder<PropertyModel, EditUrlSuggestionView, PropertyKey> {
    private final BaseSuggestionViewBinder<View> mBinder;

    public EditUrlSuggestionViewBinder() {
        mBinder = new BaseSuggestionViewBinder<>(SuggestionViewViewBinder::bind);
    }

    @Override
    public void bind(PropertyModel model, EditUrlSuggestionView view, PropertyKey propertyKey) {
        mBinder.bind(model, view.getBaseSuggestionView(), propertyKey);

        if (SuggestionCommonProperties.OMNIBOX_THEME == propertyKey) {
            Drawable drawable = OmniboxResourceProvider.resolveAttributeToDrawable(
                    view.getContext(), model.get(SuggestionCommonProperties.OMNIBOX_THEME),
                    android.R.attr.listDivider);
            view.getDivider().setBackground(drawable);
        }
    }
}
