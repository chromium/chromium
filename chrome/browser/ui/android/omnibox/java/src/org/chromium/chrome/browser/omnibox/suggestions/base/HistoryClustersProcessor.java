// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Processor for a "row UI" history clusters suggestion, i.e. one that navigates to the Journeys
 * page directly instead of hosting a chip that navigates to the Journeys page.
 */
public class HistoryClustersProcessor extends BasicSuggestionProcessor {
    private final OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;
    private final Context mContext;
    private int mJourneysActionShownPosition = -1;

    /** Delegate for HistoryClusters-related logic that omnibox code can't perform for itself. */
    public interface OpenHistoryClustersDelegate {
        void openHistoryClustersUi(String query);
    }

    /**
     * See {@link BasicSuggestionProcessor#BasicSuggestionProcessor(Context, SuggestionHost,
     * OmniboxActionDelegate, UrlBarEditingTextStateProvider, FaviconFetcher, BookmarkState)}
     */
    public HistoryClustersProcessor(OpenHistoryClustersDelegate openHistoryClustersDelegate,
            @NonNull Context context, @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull FaviconFetcher faviconFetcher, @NonNull BookmarkState bookmarkState) {
        super(context, suggestionHost, editingTextProvider, faviconFetcher, bookmarkState);
        mOpenHistoryClustersDelegate = openHistoryClustersDelegate;
        mContext = context;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);
        if (!hasFocus) {
            OmniboxMetrics.recordResumeJourneyShown(mJourneysActionShownPosition);
        }
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        if (!OmniboxFeatures.isJourneysRowUiEnabled()) {
            return false;
        }
        HistoryClustersAction action = getHistoryClustersAction(suggestion);
        if (action == null) return false;
        assert !TextUtils.isEmpty(action.query);
        return true;
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        HistoryClustersAction pedal = getHistoryClustersAction(suggestion);
        if (pedal == null) return;

        super.populateModel(suggestion, model, position);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, new SuggestionSpannable(pedal.hint));
        model.set(BaseSuggestionViewProperties.ON_CLICK,
                () -> onJourneysSuggestionClicked(pedal, position));
        model.set(BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onJourneysSuggestionClicked(pedal, position));
        SuggestionDrawableState sds =
                SuggestionDrawableState.Builder.forDrawableRes(mContext, pedal.icon.iconRes)
                        .setAllowTint(false)
                        .build();
        model.set(BaseSuggestionViewProperties.ICON, sds);
        // We want to behave like a search suggestion w.r.t. secondary text coloring.
        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, true);
        setActionButtons(model, null);
        mJourneysActionShownPosition = position;
    }

    @Override
    public boolean allowOmniboxActions() {
        return false;
    }

    private void onJourneysSuggestionClicked(HistoryClustersAction action, int position) {
        if (mOpenHistoryClustersDelegate != null) {
            String query = action.query;
            OmniboxMetrics.recordResumeJourneyClick(position);
            mOpenHistoryClustersDelegate.openHistoryClustersUi(query);
        }
    }

    /**
     * Returns the associated history clusters action for a suggestion if one exists. Returns null
     * if:
     * * No history clusters actions is present.
     * * The suggestion has 0 associated actions.
     * * The suggestion has >1 associated actions.
     * */
    private static @Nullable HistoryClustersAction getHistoryClustersAction(
            AutocompleteMatch suggestion) {
        List<OmniboxAction> actions = suggestion.getActions();
        if (actions.size() != 1) return null;
        OmniboxAction action = actions.get(0);
        if (action.actionId == OmniboxActionId.HISTORY_CLUSTERS) {
            return HistoryClustersAction.from(action);
        }
        return null;
    }
}
