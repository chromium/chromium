// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.view.View;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds chip suggestion view properties.
 * @param <T> The inner content view type being updated.
 */
public final class PedalSuggestionViewBinder<T extends View>
        implements ViewBinder<PropertyModel, PedalSuggestionView<T>, PropertyKey> {
    private final BaseSuggestionViewBinder<T> mBaseViewBinder;

    /**
     * Constructs a new pedal view binder.
     *
     * @param contentBinder The view binder for the BaseSuggestionView.
     */
    public PedalSuggestionViewBinder(ViewBinder<PropertyModel, T, PropertyKey> contentBinder) {
        mBaseViewBinder = new BaseSuggestionViewBinder<T>(contentBinder);
    }

    @Override
    public void bind(PropertyModel model, PedalSuggestionView<T> view, PropertyKey propertyKey) {
        mBaseViewBinder.bind(model, view.getBaseSuggestionView(), propertyKey);

        if (PedalSuggestionViewProperties.PEDAL == propertyKey) {
            OmniboxPedal omniboxPedal = model.get(PedalSuggestionViewProperties.PEDAL);
            view.getPedalTextView().setText(omniboxPedal.getHint());
        } else if (PedalSuggestionViewProperties.PEDAL_ICON == propertyKey) {
            view.getPedalChipView().setIcon(model.get(PedalSuggestionViewProperties.PEDAL_ICON),
                    /*tintWithTextColor=*/false);
        } else if (PedalSuggestionViewProperties.ON_PEDAL_CLICK == propertyKey) {
            view.getPedalChipView().setOnClickListener(
                    model.get(PedalSuggestionViewProperties.ON_PEDAL_CLICK));
        }
    }
}
