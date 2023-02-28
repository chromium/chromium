// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.chips.ChipViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds chip suggestion view properties.
 * @param <T> The inner content view type being updated.
 */
public final class PedalSuggestionViewBinder<T extends View>
        implements ViewBinder<PropertyModel, PedalSuggestionView<T>, PropertyKey> {
    private static final ViewBinder<PropertyModel, View, PropertyKey> NOOP_BINDER = (m, v, p) -> {};
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
            var isIncognito = model.get(SuggestionCommonProperties.COLOR_SCHEME)
                    == BrandedColorScheme.INCOGNITO;
            var chipList = model.get(PedalSuggestionViewProperties.PEDAL_LIST);
            var adapter = new PedalViewAdapter(chipList);
            adapter.registerType(PedalSuggestionViewProperties.ViewType.HEADER,
                    PedalSuggestionViewBinder::createHeaderView, NOOP_BINDER);
            adapter.registerType(PedalSuggestionViewProperties.ViewType.PEDAL_VIEW,
                    parent -> createChipView(parent, isIncognito), ChipViewBinder::bind);
            view.getPedalView().setAdapter(adapter);
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            BaseSuggestionViewBinder.applySelectableBackground(model, view);
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view.getPedalView(), model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (DropdownCommonProperties.TOP_MARGIN == propertyKey) {
            BaseSuggestionViewBinder.updateMargin(model, view);
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
