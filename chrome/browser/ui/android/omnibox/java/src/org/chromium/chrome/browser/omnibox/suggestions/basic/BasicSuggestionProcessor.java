// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.SpannableStringBuilder;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.DocumentType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxSuggestionKind;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** A class that handles model and view creation for the basic omnibox suggestions. */
@NullMarked
public class BasicSuggestionProcessor extends BaseSuggestionViewProcessor {
    private static final String TAKEOVER_SEPARATOR = " - ";

    /** Bookmarked state of a URL. */
    public interface BookmarkState {
        /**
         * @param url URL to check.
         * @return {@code true} if the given URL is bookmarked.
         */
        boolean isBookmarked(GURL url);
    }

    private final BookmarkState mBookmarkState;

    /**
     * @param uiContext Context object containing common UI dependencies.
     */
    public BasicSuggestionProcessor(AutocompleteUIContext uiContext) {
        super(uiContext);
        mBookmarkState = uiContext.bookmarkState;
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
                return R.drawable.ic_history_24dp;

            case SuggestTemplateInfo.IconType.SEARCH_LOOP_VALUE:
                return R.drawable.ic_suggestion_magnifier;

            case SuggestTemplateInfo.IconType.SEARCH_LOOP_WITH_SPARKLE_VALUE:
                return R.drawable.search_spark_black_24dp;

            case SuggestTemplateInfo.IconType.TRENDING_VALUE:
                return R.drawable.trending_up_black_24dp;

            case SuggestTemplateInfo.IconType.SUB_ARROW_RIGHT_VALUE:
                // TODO(crbug.com/437177158): Replace with the correct symbol when it's available.
                return R.drawable.ic_suggestion_magnifier;

            case SuggestTemplateInfo.IconType.GLOBE_WITH_SEARCH_LOOP_VALUE:
                return R.drawable.travel_explore_24dp;

            case SuggestTemplateInfo.IconType.BANANA_VALUE:
                return R.drawable.create_image_24dp;

            case SuggestTemplateInfo.IconType.FAVICON_VALUE:
                return R.drawable.ic_globe_24dp;

            case SuggestTemplateInfo.IconType.NOTES_SPARK_VALUE:
                return R.drawable.notes_spark;

            case SuggestTemplateInfo.IconType.DRAFT_SPARK_VALUE:
                return R.drawable.draft_spark_24dp;

            case SuggestTemplateInfo.IconType.LIGHTBULB_VALUE:
                return R.drawable.ic_lightbulb_24dp;

            case SuggestTemplateInfo.IconType.ATTACH_FILE_VALUE:
                return R.drawable.ic_attach_file_24dp;

            case SuggestTemplateInfo.IconType.SCHOOL_VALUE:
                return R.drawable.ic_school_24dp;

            case SuggestTemplateInfo.IconType.INK_PEN_VALUE:
                return R.drawable.ic_ink_pen_24dp;

            case SuggestTemplateInfo.IconType.TAB_VALUE:
                return R.drawable.tab;

            case SuggestTemplateInfo.IconType.PHOTO_SPARK_VALUE:
                return R.drawable.ic_photo_spark_24dp;

            case SuggestTemplateInfo.IconType.BOLT_VALUE:
                return R.drawable.bolt_24dp;

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
                return R.drawable.ic_history_24dp;

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
        boolean allowTint = true;
        if (suggestion.getType() == OmniboxSuggestionType.DOCUMENT_SUGGESTION) {
            icon = getDocumentIcon(suggestion.getDocumentType());
            allowTint = false;
        } else if (suggestion.getType() == OmniboxSuggestionType.STARTER_PACK) {
            int starterPackId = suggestion.getStarterPackId();
            if (starterPackId == StarterPackId.BOOKMARKS) {
                icon = R.drawable.ic_star_24dp;
            } else if (starterPackId == StarterPackId.HISTORY) {
                icon = R.drawable.ic_history_24dp;
            } else if (starterPackId == StarterPackId.TABS) {
                icon = R.drawable.switch_to_tab;
            } else if (starterPackId == StarterPackId.GEMINI) {
                icon = R.drawable.ic_spark_4c_16dp;
            }
        } else if (suggestion.getTakeoverAction() != null) {
            var action = suggestion.getTakeoverAction();
            icon = action.icon.chipIconRes;
            allowTint = action.icon.tintWithTextColor;
        }

        if (icon == 0 && suggestion.isSearchSuggestion()) {
            icon = getFallbackIconFromIconType(suggestion.getIconType());
            if (icon == 0) {
                icon =
                        getFallbackIconFromMatchTypeAndSubtypes(
                                suggestion.getType(), suggestion.getSubtypes());
            }
        }

        if (icon == 0 && mBookmarkState.isBookmarked(suggestion.getUrl())) {
            icon = R.drawable.ic_star_24dp;
        }

        return icon == 0
                ? super.getFallbackIcon(suggestion)
                : OmniboxDrawableState.forSmallIcon(mUiContext.resourceProvider, icon, allowTint);
    }

    @Override
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        super.populateModel(input, suggestion, model, position);
        final boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        final boolean isDocumentSuggestion =
                suggestion.getType() == OmniboxSuggestionType.DOCUMENT_SUGGESTION;
        final boolean isTabSearch =
                input.getPageClassification() == PageClassification.ANDROID_TAB_SEARCH_OVERLAY;
        SuggestionSpannable textLine2 = null;
        boolean urlHighlighted = false;
        @ColorInt int textLine2Color = 0;

        if (!isSearchSuggestion && !isDocumentSuggestion) {
            if (!suggestion.getUrl().isEmpty()
                    && suggestion.getType() != OmniboxSuggestionType.STARTER_PACK
                    && UrlBarData.shouldShowUrl(suggestion.getUrl(), false)) {
                textLine2 = new SuggestionSpannable(suggestion.getDisplayText());
                textLine2Color = mUiContext.resourceProvider.getSuggestionUrlTextColor();
                urlHighlighted =
                        applyHighlightToMatchRegions(
                                textLine2, suggestion.getDisplayTextClassifications());
            }
        } else {
            textLine2 = getSuggestionDescription(suggestion);
            textLine2Color = mUiContext.resourceProvider.getSuggestionSecondaryTextColor();
        }

        SuggestionSpannable textLine1 =
                getSuggestedQuery(
                        suggestion, !isSearchSuggestion && !isDocumentSuggestion, !urlHighlighted);

        applyTextColor(textLine1, mUiContext.resourceProvider.getSuggestionPrimaryTextColor());
        applyTextColor(textLine2, textLine2Color);

        // Tab search on desktop is exempt from the standard single-line desktop layout.
        if (!isTabSearch
                && OmniboxCapabilities.isDesktopPlatform()
                && !TextUtils.isEmpty(textLine2)) {
            // Separate text and url with an emdash on Desktop. Desktop shows URLs as a single line.
            var separator =
                    mUiContext.resourceProvider.getString(
                            R.string.autocomplete_match_description_separator);

            textLine1 =
                    new SuggestionSpannable(
                            new SpannableStringBuilder()
                                    .append(textLine1)
                                    .append(separator)
                                    .append(textLine2));
            textLine2 = null;
        }

        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, isSearchSuggestion);
        model.set(SuggestionViewProperties.ALLOW_WRAP_AROUND, isSearchSuggestion);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);

        String header = model.get(SuggestionCommonProperties.HEADER_TITLE);
        // 1-based index for human-readable announcements.
        int indexInGroup = model.get(SuggestionCommonProperties.INDEX_IN_GROUP) + 1;
        int totalInGroup = model.get(SuggestionCommonProperties.TOTAL_IN_GROUP);

        if (totalInGroup > 0) {
            String announcement;
            String suggestionKindStr = mContext.getString(getSuggestionKindString(suggestion));
            if (textLine2 != null && !TextUtils.isEmpty(textLine2.toString())) {
                announcement =
                        mUiContext.resourceProvider.getString(
                                R.string.acc_omnibox_suggestion_in_group_with_type_and_description,
                                textLine1.toString(),
                                textLine2.toString(),
                                suggestionKindStr,
                                String.valueOf(indexInGroup),
                                String.valueOf(totalInGroup),
                                header != null ? header : "");

            } else {
                announcement =
                        mUiContext.resourceProvider.getString(
                                R.string.acc_omnibox_suggestion_in_group_with_type,
                                textLine1.toString(),
                                suggestionKindStr,
                                String.valueOf(indexInGroup),
                                String.valueOf(totalInGroup),
                                header != null ? header : "");
            }
            model.set(SuggestionViewProperties.CONTENT_DESCRIPTION, announcement);
        }

        if (!isSearchSuggestion
                && !mBookmarkState.isBookmarked(suggestion.getUrl())
                && !isDocumentSuggestion) {
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }

        setRemoveOrRefineAction(model, input, suggestion, position);
    }

    private int getSuggestionKindString(AutocompleteMatch suggestion) {
        switch (suggestion.getSuggestionKind()) {
            case OmniboxSuggestionKind.CONVERSATION:
                return R.string.acc_omnibox_suggestion_type_conversation;
            case OmniboxSuggestionKind.SEARCH:
                return R.string.acc_omnibox_suggestion_type_search;
            case OmniboxSuggestionKind.NAVIGATION:
            default:
                return R.string.acc_omnibox_suggestion_type_navigation;
        }
    }

    protected @Nullable SuggestionSpannable getSuggestionDescription(AutocompleteMatch match) {
        if (match.getDescription() != null) {
            return new SuggestionSpannable(match.getDescription());
        }
        return null;
    }

    private SuggestionSpannable getTakeoverActionSuggestedQuery(AutocompleteMatch suggestion) {
        String contents = BidiFormatter.getInstance().unicodeWrap(suggestion.getDisplayText());
        String description = suggestion.getDescription();
        if (TextUtils.isEmpty(description)) return new SuggestionSpannable(contents);

        SpannableStringBuilder builder = new SpannableStringBuilder();
        boolean shouldSwap = suggestion.shouldSwapContentsAndDescription();
        builder.append(shouldSwap ? description : contents);
        builder.append(TAKEOVER_SEPARATOR);
        builder.append(shouldSwap ? contents : description);
        return new SuggestionSpannable(builder);
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
        if (suggestion.getTakeoverAction() != null) {
            return getTakeoverActionSuggestedQuery(suggestion);
        }

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

    private @DrawableRes int getDocumentIcon(@DocumentType int documentType) {
        switch (documentType) {
            case DocumentType.DRIVE_DOCS:
                return R.drawable.ic_drive_docs_24dp;
            case DocumentType.DRIVE_FORMS:
                return R.drawable.ic_drive_forms_24dp;
            case DocumentType.DRIVE_SHEETS:
                return R.drawable.ic_drive_sheets_24dp;
            case DocumentType.DRIVE_SLIDES:
                return R.drawable.ic_drive_slides_24dp;
            case DocumentType.DRIVE_IMAGE:
                return R.drawable.ic_drive_image_colored_24dp;
            case DocumentType.DRIVE_PDF:
                return R.drawable.ic_attach_pdf_24dp;
            case DocumentType.DRIVE_VIDEO:
                return R.drawable.ic_drive_video_colored_24dp;
            case DocumentType.DRIVE_FOLDER:
                return R.drawable.ic_drive_folder_colored_24dp;
            case DocumentType.DRIVE_OTHER:
            case DocumentType.NONE:
            default:
                return R.drawable.ic_drive_logo_24dp;
        }
    }
}
