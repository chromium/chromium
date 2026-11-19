// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.graphics.drawable.Drawable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState.IconSize;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.MatchClassification;
import org.chromium.components.omnibox.DocumentType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionKind;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProtoIntDef.IconType;
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
        return createPropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @VisibleForTesting
    @DrawableRes
    int getFallbackIconFromIconType(@IconType int iconType) {
        switch (iconType) {
            case IconType.ICON_TYPE_UNSPECIFIED:
                return 0;

            case IconType.HISTORY:
                return R.drawable.ic_history_24dp;

            case IconType.SEARCH_LOOP:
                return R.drawable.ic_suggestion_magnifier;

            case IconType.SEARCH_LOOP_WITH_SPARKLE:
                return R.drawable.search_spark_black_24dp;

            case IconType.TRENDING:
                return R.drawable.trending_up_black_24dp;

            case IconType.SUB_ARROW_RIGHT:
                // TODO(crbug.com/437177158): Replace with the correct symbol when it's available.
                return R.drawable.ic_suggestion_magnifier;

            case IconType.GLOBE_WITH_SEARCH_LOOP:
                return R.drawable.travel_explore_24dp;

            case IconType.BANANA:
                return R.drawable.create_image_24dp;

            case IconType.FAVICON:
                return R.drawable.ic_globe_24dp;

            case IconType.NOTES_SPARK:
                return R.drawable.notes_spark;

            case IconType.DRAFT_SPARK:
                return R.drawable.draft_spark_24dp;

            case IconType.LIGHTBULB:
                return R.drawable.ic_lightbulb_24dp;

            case IconType.ATTACH_FILE:
                return R.drawable.ic_attach_file_24dp;

            case IconType.SCHOOL:
                return R.drawable.ic_school_24dp;

            case IconType.INK_PEN:
                return R.drawable.ic_ink_pen_24dp;

            case IconType.TAB:
                return R.drawable.tab;

            case IconType.PHOTO_SPARK:
                return R.drawable.ic_photo_spark_24dp;

            case IconType.BOLT:
                return R.drawable.bolt_24dp;

            case IconType.IMAGE_CREATE:
                return R.drawable.image_create_24dp;

            case IconType.PROMPT_SUGGESTION:
                return R.drawable.ic_prompt_suggestion_24dp;

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
        OmniboxDrawableState vectorIcon = getVectorIcon(suggestion);
        if (vectorIcon != null) return vectorIcon;

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
                icon = R.drawable.ic_spark_24dp;
            }
        } else if (suggestion.getTakeoverAction() != null) {
            var action = suggestion.getTakeoverAction();
            if (action.icon.chipIconRes != 0) {
                icon = action.icon.chipIconRes;
                allowTint = action.icon.tintWithTextColor;
            }
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

    /**
     * When {@link OmniboxFeatures#isNativeVectorIconsEnabled()}, returns the suggestion's native
     * vector icon presented as a medium icon, or {@code null} if the icon should be sourced from
     * XML drawables instead. Document suggestions keep their multi-color Android drawables.
     *
     * @param suggestion The suggestion to retrieve the vector icon for.
     */
    private @Nullable OmniboxDrawableState getVectorIcon(AutocompleteMatch suggestion) {
        if (!OmniboxFeatures.isNativeVectorIconsEnabled()) return null;
        OmniboxImageSupplier imageSupplier = mUiContext.imageSupplier;
        if (imageSupplier == null) return null;
        if (suggestion.getType() == OmniboxSuggestionType.DOCUMENT_SUGGESTION) return null;

        Drawable drawable =
                imageSupplier.getVectorIcon(
                        suggestion,
                        mBookmarkState.isBookmarked(suggestion.getUrl()),
                        mUiContext.resourceProvider.getEdgeSizeDp(IconSize.MEDIUM));
        return drawable == null ? null : OmniboxDrawableState.forVectorIcon(drawable);
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
        model.set(SuggestionViewProperties.TEXT_LINE_1_WRAP, suggestion.shouldWrapPrimaryText());
        model.set(SuggestionViewProperties.TEXT_LINE_2_WRAP, suggestion.shouldWrapSecondaryText());
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);

        String header = model.get(SuggestionCommonProperties.HEADER_TITLE);
        // 1-based index for human-readable announcements.
        int indexInGroup = model.get(SuggestionCommonProperties.INDEX_IN_GROUP) + 1;
        int totalInGroup = model.get(SuggestionCommonProperties.TOTAL_IN_GROUP);

        if (totalInGroup > 0) {
            // Formatting the announcement is the single most expensive part of building this
            // model and is only ever consumed by accessibility services. Capture the cheap
            // ingredients and defer the formatting until the binder actually needs the string.
            // Local copies are needed because textLine1/textLine2 are reassigned above.
            SuggestionSpannable line1 = textLine1;
            SuggestionSpannable line2 = textLine2;
            @OmniboxSuggestionKind int suggestionKind = suggestion.getSuggestionKind();
            model.set(
                    SuggestionViewProperties.CONTENT_DESCRIPTION_SUPPLIER,
                    () ->
                            buildContentDescription(
                                    line1,
                                    line2,
                                    suggestionKind,
                                    indexInGroup,
                                    totalInGroup,
                                    header));
        }

        if (!isSearchSuggestion
                && !mBookmarkState.isBookmarked(suggestion.getUrl())
                && !isDocumentSuggestion) {
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }

        setRemoveOrRefineAction(model, input, suggestion, position);
    }

    /**
     * Formats the accessibility announcement for a suggestion row.
     *
     * <p>This is intentionally invoked lazily (see {@link
     * SuggestionViewProperties#CONTENT_DESCRIPTION_SUPPLIER}) because it is comparatively expensive
     * and only needed when an accessibility service inspects the row.
     *
     * @param textLine1 The primary text of the suggestion.
     * @param textLine2 The secondary text of the suggestion, if any.
     * @param suggestionKind The {@link OmniboxSuggestionKind} of the suggestion.
     * @param indexInGroup 1-based index of the suggestion within its group.
     * @param totalInGroup Total number of suggestions within the group.
     * @param header The group header title, if any.
     * @return The formatted announcement.
     */
    private String buildContentDescription(
            SuggestionSpannable textLine1,
            @Nullable SuggestionSpannable textLine2,
            @OmniboxSuggestionKind int suggestionKind,
            int indexInGroup,
            int totalInGroup,
            @Nullable String header) {
        String suggestionKindStr = mContext.getString(getSuggestionKindString(suggestionKind));
        if (textLine2 != null && !TextUtils.isEmpty(textLine2)) {
            return mUiContext.resourceProvider.getString(
                    R.string.acc_omnibox_suggestion_in_group_with_type_and_description,
                    textLine1.toString(),
                    textLine2.toString(),
                    suggestionKindStr,
                    String.valueOf(indexInGroup),
                    String.valueOf(totalInGroup),
                    header != null ? header : "");
        } else {
            return mUiContext.resourceProvider.getString(
                    R.string.acc_omnibox_suggestion_in_group_with_type,
                    textLine1.toString(),
                    suggestionKindStr,
                    String.valueOf(indexInGroup),
                    String.valueOf(totalInGroup),
                    header != null ? header : "");
        }
    }

    private @StringRes int getSuggestionKindString(@OmniboxSuggestionKind int suggestionKind) {
        switch (suggestionKind) {
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
        // Non-search takeover matches (e.g., Lens pedal) inline "contents - description" on line
        // 1; search/document suggestions already populate `textLine2` with the description.
        if (showDescriptionIfPresent && suggestion.getTakeoverAction() != null) {
            return getTakeoverActionSuggestedQuery(suggestion);
        }

        String suggestedQuery = null;
        List<MatchClassification> classifications;
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
            classifications = new ArrayList<MatchClassification>();
            classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
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
