// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that handles model and view creation for the pedal omnibox suggestion.
 */
public class PedalSuggestionProcessor extends BasicSuggestionProcessor {
    private final SuggestionHost mSuggestionHost;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public PedalSuggestionProcessor(@NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull Supplier<LargeIconBridge> iconBridgeSupplier,
            @NonNull BookmarkState bookmarkState) {
        super(context, suggestionHost, editingTextProvider, iconBridgeSupplier, bookmarkState);
        mSuggestionHost = suggestionHost;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getOmniboxPedal() != null;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.PEDAL_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(PedalSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setPedal(model, suggestion.getOmniboxPedal());
    }

    /**
     * Setup pedals base on the suggestion.
     *
     * @param model Property model to update.
     * @param omniboxPedal OmniboxPedal for the suggestion.
     */
    protected void setPedal(PropertyModel model, @NonNull OmniboxPedal omniboxPedal) {
        model.set(PedalSuggestionViewProperties.PEDAL, omniboxPedal);
        model.set(PedalSuggestionViewProperties.PEDAL_ICON, getPedalIcon(omniboxPedal));
        model.set(PedalSuggestionViewProperties.ON_PEDAL_CLICK,
                v -> mSuggestionHost.onPedalClicked(omniboxPedal.getID()));
    }

    /**
     * Get default icon for pedal suggestion.
     * @param omniboxPedal OmniboxPedal for the suggestion.
     * @return The icon's resource id.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @DrawableRes
    int getPedalIcon(@NonNull OmniboxPedal omniboxPedal) {
        return R.drawable.ic_google_round;
    }
}