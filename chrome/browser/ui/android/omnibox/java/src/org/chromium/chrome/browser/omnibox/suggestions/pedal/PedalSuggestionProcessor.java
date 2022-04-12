// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionViewProperties.PedalIcon;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/**
 * A class that handles model and view creation for the pedal omnibox suggestion.
 */
public class PedalSuggestionProcessor extends BasicSuggestionProcessor {
    // Only show pedals when the suggestion is on the top 3 suggestions.
    private static final int PEDAL_MAX_SHOW_POSITION = 3;

    private final @NonNull OmniboxPedalDelegate mOmniboxPedalDelegate;
    private final @NonNull AutocompleteDelegate mAutocompleteDelegate;
    private @NonNull Set<Integer> mLastVisiblePedals = new ArraySet<>();

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     * @param iconBridgeSupplier A means of accessing the large icon bridge.
     * @param bookmarkBridgeSupplier A means of accessing the bookmark information.
     * @param omniboxPedalDelegate A delegate that will responsible for pedals.
     */
    public PedalSuggestionProcessor(@NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull Supplier<LargeIconBridge> iconBridgeSupplier,
            @NonNull BookmarkState bookmarkState,
            @NonNull OmniboxPedalDelegate omniboxPedalDelegate,
            @NonNull AutocompleteDelegate autocompleteDelegate) {
        super(context, suggestionHost, editingTextProvider, iconBridgeSupplier, bookmarkState);
        mOmniboxPedalDelegate = omniboxPedalDelegate;
        mAutocompleteDelegate = autocompleteDelegate;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getOmniboxPedal() != null && position < PEDAL_MAX_SHOW_POSITION;
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

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            recordPedalShownForAllPedals();
        }
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
        final int pedalID = omniboxPedal.getPedalID();
        model.set(PedalSuggestionViewProperties.ON_PEDAL_CLICK, v -> {
            SuggestionsMetrics.recordPedalUsed(pedalID);
            mOmniboxPedalDelegate.executeAction(pedalID);
            mAutocompleteDelegate.clearOmniboxFocus();
        });
        model.set(
                BaseSuggestionViewProperties.DENSITY, BaseSuggestionViewProperties.Density.COMPACT);

        mLastVisiblePedals.add(pedalID);
    }

    /**
     * Get default icon for pedal suggestion.
     * @param omniboxPedal OmniboxPedal for the suggestion.
     * @return The icon's information.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    PedalIcon getPedalIcon(@NonNull OmniboxPedal omniboxPedal) {
        return mOmniboxPedalDelegate.getPedalIcon(omniboxPedal.getPedalID());
    }

    /**
     * Record the pedals shown for all pedal types.
     */
    private void recordPedalShownForAllPedals() {
        for (Integer pedal : mLastVisiblePedals) {
            SuggestionsMetrics.recordPedalShown(pedal);
        }
        mLastVisiblePedals.clear();
    }
}