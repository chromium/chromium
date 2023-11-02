// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionViewProperties.PedalIcon;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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
            final String hint = omniboxPedal.getHint();
            final String contentDescription =
                    view.getContext().getString(R.string.accessibility_omnibox_pedal, hint);
            view.getPedalTextView().setText(hint);
            view.getPedalTextView().setContentDescription(contentDescription);
            final @BrandedColorScheme int brandedColorScheme =
                    model.get(SuggestionCommonProperties.COLOR_SCHEME);
            view.getPedalTextView().setTextColor(
                    OmniboxResourceProvider.getSuggestionPrimaryTextColor(
                            view.getContext(), brandedColorScheme));
        } else if (PedalSuggestionViewProperties.PEDAL_ICON == propertyKey) {
            PedalIcon icon = model.get(PedalSuggestionViewProperties.PEDAL_ICON);
            view.getPedalChipView().setIcon(icon.iconRes, icon.tintWithTextColor);
            view.getPedalChipView().setBackgroundColor(Color.TRANSPARENT);
        } else if (PedalSuggestionViewProperties.ON_PEDAL_CLICK == propertyKey) {
            view.getPedalChipView().setOnClickListener(
                    model.get(PedalSuggestionViewProperties.ON_PEDAL_CLICK));
        } else if (SuggestionCommonProperties.COLOR_SCHEME == propertyKey) {
            Drawable backgroundDrawable =
                    BaseSuggestionViewBinder.getSelectableBackgroundDrawable(view, model);
            view.getPedalView().setBackground(backgroundDrawable);
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view.getPedalView(), model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (DropdownCommonProperties.BG_TOP_CORNER_ROUNDED == propertyKey) {
            BaseSuggestionViewBinder.updateBackground(model, view);
        } else if (DropdownCommonProperties.TOP_MARGIN == propertyKey) {
            BaseSuggestionViewBinder.updateMargin(model, view);
        }
    }
}
