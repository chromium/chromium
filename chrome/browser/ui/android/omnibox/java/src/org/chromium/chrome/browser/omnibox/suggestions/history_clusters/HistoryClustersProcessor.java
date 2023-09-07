// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.history_clusters;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
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
    private int mJourneysActionShownPosition = -1;

    /** Delegate for HistoryClusters-related logic that omnibox code can't perform for itself. */
    public interface OpenHistoryClustersDelegate {
        void openHistoryClustersUi(String query);
    }

    /**
     * See {@link BasicSuggestionProcessor#BasicSuggestionProcessor(Context, SuggestionHost,
     * OmniboxActionDelegate, UrlBarEditingTextStateProvider, OmniboxImageSupplier, BookmarkState)}
     */
    public HistoryClustersProcessor(OpenHistoryClustersDelegate openHistoryClustersDelegate,
            @NonNull Context context, @NonNull SuggestionHost suggestionHost,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull OmniboxImageSupplier imageSupplier, @NonNull BookmarkState bookmarkState) {
        super(context, suggestionHost, editingTextProvider, imageSupplier, bookmarkState);
        mOpenHistoryClustersDelegate = openHistoryClustersDelegate;
    }

    @Override
    public void onOmniboxSessionStateChange(boolean activated) {
        super.onOmniboxSessionStateChange(activated);
        if (!activated) {
            OmniboxMetrics.recordResumeJourneyShown(mJourneysActionShownPosition);
        }
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        if (!OmniboxFeatures.isJourneysRowUiEnabled()) {
            return false;
        }
        return getHistoryClustersAction(suggestion) != null;
    }

    @Override
    protected OmniboxDrawableState getFallbackIcon(AutocompleteMatch match) {
        var action = getHistoryClustersAction(match);
        return OmniboxDrawableState.forSmallIcon(mContext, action.icon.iconRes, false);
    }

    @Override
    protected SuggestionSpannable getSuggestionDescription(AutocompleteMatch match) {
        return new SuggestionSpannable(getHistoryClustersAction(match).hint);
    }

    @Override
    public void populateModel(AutocompleteMatch match, PropertyModel model, int position) {
        HistoryClustersAction action = getHistoryClustersAction(match);
        super.populateModel(match, model, position);
        model.set(BaseSuggestionViewProperties.ON_CLICK,
                () -> onJourneysSuggestionClicked(action, position));
        model.set(BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onJourneysSuggestionClicked(action, position));
        // We want to behave like a search suggestion w.r.t. secondary text coloring.
        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, true);
        setActionButtons(model, null);
        mJourneysActionShownPosition = position;
    }

    @Override
    protected boolean allowOmniboxActions() {
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
