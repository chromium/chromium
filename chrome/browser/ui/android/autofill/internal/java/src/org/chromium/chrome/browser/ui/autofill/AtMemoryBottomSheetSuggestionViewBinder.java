// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_FLYOUT_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_SUGGESTION_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.TITLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties to an {@link AtMemoryBottomSheetSuggestionView}. */
@NullMarked
public class AtMemoryBottomSheetSuggestionViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link AtMemoryBottomSheetSuggestionView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bind(
            PropertyModel model, AtMemoryBottomSheetSuggestionView view, PropertyKey propertyKey) {
        if (propertyKey == ICON) {
            view.setIcon(model.get(ICON));
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else if (propertyKey == DETAILS) {
            view.setDetails(model.get(DETAILS));
        } else if (propertyKey == ON_SUGGESTION_CLICKED) {
            view.setSuggestionClickListener(model.get(ON_SUGGESTION_CLICKED));
        } else if (propertyKey == ON_FLYOUT_CLICKED) {
            view.setFlyoutClickListener(model.get(ON_FLYOUT_CLICKED));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }
}
