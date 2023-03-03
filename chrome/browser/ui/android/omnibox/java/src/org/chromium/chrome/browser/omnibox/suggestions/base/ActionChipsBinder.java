// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.chips.ChipViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds ActionChipsView properties.
 */
public final class ActionChipsBinder {
    private static final ViewBinder<PropertyModel, View, PropertyKey> NOOP_BINDER = (m, v, p) -> {};

    public static void bind(PropertyModel model, ActionChipsView view, PropertyKey propertyKey) {
        if (ActionChipsProperties.ACTION_CHIPS == propertyKey) {
            var isIncognito = model.get(SuggestionCommonProperties.COLOR_SCHEME)
                    == BrandedColorScheme.INCOGNITO;
            var chipList = model.get(ActionChipsProperties.ACTION_CHIPS);
            ActionChipsAdapter adapter = null;
            int actionChipsVisibility = View.GONE;

            if (chipList != null) {
                adapter = new ActionChipsAdapter(chipList);
                adapter.registerType(ActionChipsProperties.ViewType.HEADER,
                        ActionChipsBinder::createHeaderView, NOOP_BINDER);
                adapter.registerType(ActionChipsProperties.ViewType.CHIP,
                        parent -> createChipView(parent, isIncognito), ChipViewBinder::bind);
                actionChipsVisibility = View.VISIBLE;
            }
            view.setAdapter(adapter);
            view.setVisibility(actionChipsVisibility);
        }
    }

    /**
     * Create a view element that provides horizontal alignment.
     */
    private static View createHeaderView(@NonNull ViewGroup parent) {
        var res = parent.getResources();

        boolean showModernizedSuggestionsList =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(parent.getContext());

        int actionChipHeaderWidth =
                res.getDimensionPixelSize(showModernizedSuggestionsList
                                ? R.dimen.omnibox_suggestion_icon_area_size_modern
                                : R.dimen.omnibox_suggestion_icon_area_size)
                -
                // We apply spacing to every element, 1/2 on the left and 1/2 on the right.
                // Lead-in header receives both left and right spacing, but we also need to erase
                // the space before the first chip.
                res.getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing) * 3 / 2;

        var view = new View(parent.getContext());
        view.setMinimumWidth(actionChipHeaderWidth);
        return view;
    }

    public static ChipView createChipView(@NonNull ViewGroup parent, boolean isIncognito) {
        return new ChipView(parent.getContext(),
                isIncognito ? R.style.OmniboxIncognitoActionChipThemeOverlay
                            : R.style.OmniboxActionChipThemeOverlay);
    }
}
