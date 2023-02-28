// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.collection.ArraySet;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Set;

/**
 * A class that handles model and view creation for the pedal omnibox suggestion.
 */
public class PedalSuggestionProcessor extends BasicSuggestionProcessor {
    // Only show pedals when the suggestion is on the top 3 suggestions.
    private static final int PEDAL_MAX_SHOW_POSITION = 3;

    private final @NonNull Context mContext;
    private final @NonNull OmniboxPedalDelegate mOmniboxPedalDelegate;
    private final @NonNull AutocompleteDelegate mAutocompleteDelegate;
    private @NonNull Set<Integer> mLastVisiblePedals = new ArraySet<>();
    private int mJourneysActionShownPosition = -1;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     * @param faviconFetcher A means of accessing the large icon bridge.
     * @param bookmarkBridgeSupplier A means of accessing the bookmark information.
     * @param omniboxPedalDelegate A delegate that will responsible for pedals.
     */
    public PedalSuggestionProcessor(@NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull FaviconFetcher faviconFetcher, @NonNull BookmarkState bookmarkState,
            @NonNull OmniboxPedalDelegate omniboxPedalDelegate,
            @NonNull AutocompleteDelegate autocompleteDelegate) {
        super(context, suggestionHost, editingTextProvider, faviconFetcher, bookmarkState);
        mContext = context;
        mOmniboxPedalDelegate = omniboxPedalDelegate;
        mAutocompleteDelegate = autocompleteDelegate;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getActions().size() > 0 && position < PEDAL_MAX_SHOW_POSITION;
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
        setPedalList(model, suggestion.getActions(), position);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            recordActionsShown();
        }
    }

    /**
     * Setup pedal list base on the suggestion.
     *
     * @param model Property model to update.
     * @param omniboxPedalList List of OmniboxPedal for the suggestion.
     * @param position The position for the list of OmniboxPedal among omnibox suggestions.
     */
    protected void setPedalList(
            PropertyModel model, @NonNull List<OmniboxPedal> omniboxPedalList, int position) {
        var modelList = new ModelList();

        // The header item increases lead-in padding before the first actual chip is shown.
        // In default state, the chips will align with the suggestion text, but when scrolled
        // the chips may show up under the decoration.
        modelList.add(
                new ListItem(PedalSuggestionViewProperties.ViewType.HEADER, new PropertyModel()));

        for (OmniboxPedal chip : omniboxPedalList) {
            final var chipIcon = mOmniboxPedalDelegate.getIcon(chip);
            final var chipModel =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.TEXT, chip.getHint())
                            .with(ChipProperties.CONTENT_DESCRIPTION,
                                    mContext.getString(
                                            R.string.accessibility_omnibox_pedal, chip.getHint()))
                            .with(ChipProperties.ENABLED, true)
                            .with(ChipProperties.CLICK_HANDLER, m -> executeAction(chip, position))
                            .with(ChipProperties.ICON, chipIcon.iconRes)
                            .with(ChipProperties.APPLY_ICON_TINT, chipIcon.tintWithTextColor)
                            .build();

            modelList.add(
                    new ListItem(PedalSuggestionViewProperties.ViewType.PEDAL_VIEW, chipModel));

            if (chip.hasPedalId()) {
                mLastVisiblePedals.add(chip.getPedalID());
            } else if (chip.hasActionId()
                    && chip.getActionID() == OmniboxActionType.HISTORY_CLUSTERS) {
                mJourneysActionShownPosition = position;
            }
        }

        model.set(PedalSuggestionViewProperties.PEDAL_LIST, modelList);
        model.set(
                BaseSuggestionViewProperties.DENSITY, BaseSuggestionViewProperties.Density.COMPACT);
    }

    void executeAction(@NonNull OmniboxPedal omniboxPedal, int position) {
        if (omniboxPedal.hasActionId()
                && omniboxPedal.getActionID() == OmniboxActionType.HISTORY_CLUSTERS) {
            RecordHistogram.recordEnumeratedHistogram("Omnibox.SuggestionUsed.ResumeJourney",
                    position, SuggestionsMetrics.MAX_AUTOCOMPLETE_POSITION);
        }
        mAutocompleteDelegate.clearOmniboxFocus();
        mOmniboxPedalDelegate.execute(omniboxPedal);
    }

    /**
     * Record the actions shown for all action types (Journeys + any pedals).
     */
    private void recordActionsShown() {
        for (Integer pedal : mLastVisiblePedals) {
            SuggestionsMetrics.recordPedalShown(pedal);
        }
        if (mJourneysActionShownPosition != -1) {
            RecordHistogram.recordExactLinearHistogram("Omnibox.ResumeJourneyShown",
                    mJourneysActionShownPosition, SuggestionsMetrics.MAX_AUTOCOMPLETE_POSITION);
            mJourneysActionShownPosition = -1;
        }

        mLastVisiblePedals.clear();
    }
}
