// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** A class that handles model and view creation for the basic omnibox suggestions. */
public class BasicSuggestionProcessor extends BaseSuggestionViewProcessor {
    /** Bookmarked state of a URL */
    public interface BookmarkState {
        /**
         * @param url URL to check.
         * @return {@code true} if the given URL is bookmarked.
         */
        boolean isBookmarked(GURL url);
    }
    private final @NonNull UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final @NonNull BookmarkState mBookmarkState;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param actionChipsDelegate Delegate that gives us information what action chips should look
     *         like and how to execute them.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     * @param faviconFetcher Fetcher for favicon images.
     * @param bookmarkState Provider of information about whether a given url is bookmarked.
     */
    public BasicSuggestionProcessor(@NonNull Context context,
            @NonNull SuggestionHost suggestionHost,
            @Nullable ActionChipsDelegate actionChipsDelegate,
            @NonNull UrlBarEditingTextStateProvider editingTextProvider,
            @NonNull FaviconFetcher faviconFetcher, @NonNull BookmarkState bookmarkState) {
        super(context, suggestionHost, actionChipsDelegate, faviconFetcher);

        mUrlBarEditingTextProvider = editingTextProvider;
        mBookmarkState = bookmarkState;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.DEFAULT;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    /**
     * Returns suggestion icon to be presented for specified omnibox suggestion.
     *
     * This method returns the stock icon type to be attached to the Suggestion.
     * Note that the stock icons do not include Favicon - Favicon is only declared
     * when we know we have a valid and large enough site favicon to present.
     */
    private @DrawableRes int getSuggestionIcon(AutocompleteMatch suggestion) {
        if (suggestion.isSearchSuggestion()) {
            switch (suggestion.getType()) {
                case OmniboxSuggestionType.VOICE_SUGGEST:
                    return R.drawable.btn_mic;

                case OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED:
                case OmniboxSuggestionType.SEARCH_HISTORY:
                    return R.drawable.ic_history_googblue_24dp;

                default:
                    if (suggestion.getSubtypes().contains(/* SUBTYPE_TRENDS = */ 143)) {
                        return R.drawable.trending_up_black_24dp;
                    }
                    return R.drawable.ic_suggestion_magnifier;
            }
        } else {
            if (mBookmarkState.isBookmarked(suggestion.getUrl())) {
                return R.drawable.btn_star;
            } else {
                return R.drawable.ic_globe_24dp;
            }
        }
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        final @OmniboxSuggestionType int suggestionType = suggestion.getType();
        final boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        SuggestionSpannable textLine2 = null;
        boolean urlHighlighted = false;

        if (!isSearchSuggestion) {
            if (!suggestion.getUrl().isEmpty()
                    && UrlBarData.shouldShowUrl(suggestion.getUrl(), false)) {
                SuggestionSpannable str = new SuggestionSpannable(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
            }
        } else if (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE) {
            textLine2 = new SuggestionSpannable(suggestion.getDescription());
        }

        final SuggestionSpannable textLine1 =
                getSuggestedQuery(suggestion, !isSearchSuggestion, !urlHighlighted);

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(getContext(), getSuggestionIcon(suggestion))
                        .setAllowTint(true)
                        .build());

        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, isSearchSuggestion);
        model.set(SuggestionViewProperties.ALLOW_WRAP_AROUND, isSearchSuggestion);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);
        fetchSuggestionFavicon(model, suggestion.getUrl());

        if (!mUrlBarEditingTextProvider.getTextWithoutAutocomplete().trim().equalsIgnoreCase(
                    suggestion.getDisplayText())) {
            setTabSwitchOrRefineAction(model, suggestion, position);
        }
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private SuggestionSpannable getSuggestedQuery(AutocompleteMatch suggestion,
            boolean showDescriptionIfPresent, boolean shouldHighlight) {
        String suggestedQuery = null;
        List<AutocompleteMatch.MatchClassification> classifications;
        if (showDescriptionIfPresent && !suggestion.getUrl().isEmpty()
                && !TextUtils.isEmpty(suggestion.getDescription())) {
            suggestedQuery = suggestion.getDescription();
            classifications = suggestion.getDescriptionClassifications();
        } else {
            suggestedQuery = suggestion.getDisplayText();
            classifications = suggestion.getDisplayTextClassifications();
        }
        if (suggestedQuery == null) {
            assert false : "Invalid suggestion sent with no displayable text";
            suggestedQuery = "";
            classifications = new ArrayList<AutocompleteMatch.MatchClassification>();
            classifications.add(
                    new AutocompleteMatch.MatchClassification(0, MatchClassificationStyle.NONE));
        }

        SuggestionSpannable str = new SuggestionSpannable(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }
}
