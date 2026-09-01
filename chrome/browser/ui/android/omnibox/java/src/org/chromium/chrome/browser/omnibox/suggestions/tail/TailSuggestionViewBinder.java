// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the tail suggestion view. */
@NullMarked
public class TailSuggestionViewBinder extends BaseSuggestionViewBinder<TailSuggestionView> {

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    protected void bindContent(
            PropertyModel model, TailSuggestionView view, PropertyKey propertyKey) {
        if (TailSuggestionViewProperties.ALIGNMENT_MANAGER == propertyKey) {
            view.setAlignmentManager(model.get(TailSuggestionViewProperties.ALIGNMENT_MANAGER));
        } else if (propertyKey == SuggestionCommonProperties.COLOR_SCHEME) {
            final @ColorInt int color = getResourceProvider(model).getSuggestionPrimaryTextColor();
            view.setTextColor(color);
        } else if (propertyKey == TailSuggestionViewProperties.FILL_INTO_EDIT) {
            view.setFullText(model.get(TailSuggestionViewProperties.FILL_INTO_EDIT));
        } else if (propertyKey == TailSuggestionViewProperties.TEXT) {
            view.setTailText(model.get(TailSuggestionViewProperties.TEXT));
        }
    }
}
