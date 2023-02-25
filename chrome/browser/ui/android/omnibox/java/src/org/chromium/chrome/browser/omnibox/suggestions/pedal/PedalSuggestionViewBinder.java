// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.view.View;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.components.browser_ui.widget.chips.ChipViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.List;

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

        if (PedalSuggestionViewProperties.PEDAL_LIST == propertyKey) {
            List<ListItem> chipList = model.get(PedalSuggestionViewProperties.PEDAL_LIST);
            // TODO(crbug/1418077): Turn this into a proper MVC.
            // We're introducing support for multiple chips (not landed yet) and migrating from
            // FrameView to RecyclerView.
            PropertyModel chipModel = chipList.get(0).model;
            for (var property : chipModel.getAllSetProperties()) {
                ChipViewBinder.bind(chipModel, view.getPedalChipView(), property);
            }
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            BaseSuggestionViewBinder.applySelectableBackground(model, view.getPedalView());
            // Apply changes to Chips as well.
            // Currently chips work well in light and dark themes, but poorly in an Incognito mode.
            // Need to research how to plumb styling properly so that Incognito mode is properly
            // reflected.
            List<ListItem> chipList = model.get(PedalSuggestionViewProperties.PEDAL_LIST);
            var brandedColorScheme = model.get(SuggestionCommonProperties.COLOR_SCHEME);
            var chip = view.getPedalChipView();
            chip.setBackgroundColor(BaseSuggestionViewBinder.getSuggestionBackgroundColor(
                    model, view.getContext()));
            chip.setTextColor(OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                    view.getContext(), brandedColorScheme));
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view.getPedalView(), model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (DropdownCommonProperties.TOP_MARGIN == propertyKey) {
            BaseSuggestionViewBinder.updateMargin(model, view);
        }
    }
}
