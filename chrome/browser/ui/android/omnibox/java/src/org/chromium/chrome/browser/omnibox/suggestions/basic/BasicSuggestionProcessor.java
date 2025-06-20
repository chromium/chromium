// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.GroupsProto.GroupId;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/** A class that handles model and view creation for the basic omnibox suggestions. */
@NullMarked
public class BasicSuggestionProcessor extends BaseSuggestionViewProcessor {
    /** Bookmarked state of a URL */
    public interface BookmarkState {
        /**
         * @param url URL to check.
         * @return {@code true} if the given URL is bookmarked.
         */
        boolean isBookmarked(GURL url);
    }

    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final BookmarkState mBookmarkState;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     * @param imageSupplier Supplier of suggestion images.
     * @param bookmarkState Provider of information about whether a given url is bookmarked.
     */
    public BasicSuggestionProcessor(
            Context context,
            SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider,
            Optional<OmniboxImageSupplier> imageSupplier,
            BookmarkState bookmarkState) {
        super(context, suggestionHost, imageSupplier);

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

    @VisibleForTesting
    @DrawableRes
    int getFallbackIconFromIconType(/* SuggestTemplateInfo.IconType */ int iconType) {
        switch (iconType) {
            case SuggestTemplateInfo.IconType.ICON_TYPE_UNSPECIFIED_VALUE:
                return 0;

            case SuggestTemplateInfo.IconType.HISTORY_VALUE:
                return R.drawable.ic_history_googblue_24dp;

            case SuggestTemplateInfo.IconType.SEARCH_LOOP_VALUE:
                return R.drawable.ic_suggestion_magnifier;

            case SuggestTemplateInfo.IconType.SEARCH_LOOP_WITH_SPARKLE_VALUE:
                return R.drawable.search_spark_black_24dp;

            case SuggestTemplateInfo.IconType.TRENDING_VALUE:
                return R.drawable.trending_up_black_24dp;

            default: // Icon type is specified, but not recognized
                assert false : "Unrecognized IconType: " + iconType;
                return 0;
        }
    }

    private @DrawableRes int getFallbackIconFromMatchTypeAndSubtypes(
            @OmniboxSuggestionType int suggestionType, Set<Integer> suggestionSubtypes) {
        switch (suggestionType) {
            case OmniboxSuggestionType.VOICE_SUGGEST:
                return R.drawable.ic_mic_white_24dp;

            case OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED:
            case OmniboxSuggestionType.SEARCH_HISTORY:
                return R.drawable.ic_history_googblue_24dp;

            default:
                if (suggestionSubtypes.contains(/* SUBTYPE_TRENDS= */ 143)) {
                    return R.drawable.trending_up_black_24dp;
                }
        }
        return 0;
    }

    @Override
    protected OmniboxDrawableState getFallbackIcon(AutocompleteMatch suggestion) {
        @DrawableRes int icon = 0;
        if (suggestion.isSearchSuggestion()) {
            icon = getFallbackIconFromIconType(suggestion.getIconType());
            if (icon == 0) {
                icon =
                        getFallbackIconFromMatchTypeAndSubtypes(
                                suggestion.getType(), suggestion.getSubtypes());
            }
        } else if (
        /* !isSearchSuggestion && */ mBookmarkState.isBookmarked(suggestion.getUrl())) {
            icon = R.drawable.star_outline_24dp;
        }

        return icon == 0
                ? super.getFallbackIcon(suggestion)
                : OmniboxDrawableState.forSmallIcon(mContext, icon, true);
    }

    @Override
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        super.populateModel(input, suggestion, model, position);
        final boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        SuggestionSpannable textLine2 = null;
        boolean urlHighlighted = false;

        if (!isSearchSuggestion) {
            if (!suggestion.getUrl().isEmpty()
                    && UrlBarData.shouldShowUrl(suggestion.getUrl(), false)) {
                SuggestionSpannable str = new SuggestionSpannable(suggestion.getDisplayText());
                urlHighlighted =
                        applyHighlightToMatchRegions(
                                str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
            }
        } else {
            textLine2 = getSuggestionDescription(suggestion);
        }

        final SuggestionSpannable textLine1 =
                getSuggestedQuery(suggestion, !isSearchSuggestion, !urlHighlighted);

        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, isSearchSuggestion);
        model.set(SuggestionViewProperties.ALLOW_WRAP_AROUND, isSearchSuggestion);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);
        if (!isSearchSuggestion && !mBookmarkState.isBookmarked(suggestion.getUrl())) {
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }

        if (suggestion.getType() != OmniboxSuggestionType.TILE_SUGGESTION
                && !mUrlBarEditingTextProvider
                        .getTextWithoutAutocomplete()
                        .trim()
                        .equalsIgnoreCase(suggestion.getDisplayText())) {
            setTabSwitchOrRefineAction(model, input, suggestion, position);
        }
    }

    protected @Nullable SuggestionSpannable getSuggestionDescription(AutocompleteMatch match) {
        if (match.getGroupId() == GroupId.GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA.getNumber()) {
            return new SuggestionSpannable(match.getDescription());
        }
        return null;
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     *
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if the
     *     item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private SuggestionSpannable getSuggestedQuery(
            AutocompleteMatch suggestion,
            boolean showDescriptionIfPresent,
            boolean shouldHighlight) {
        String suggestedQuery = null;
        List<AutocompleteMatch.MatchClassification> classifications;
        if (showDescriptionIfPresent
                && !suggestion.getUrl().isEmpty()
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
