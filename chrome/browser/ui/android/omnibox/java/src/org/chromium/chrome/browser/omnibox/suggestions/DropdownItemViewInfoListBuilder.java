// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.dividerline.DividerLineProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.history_clusters.HistoryClustersProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.history_clusters.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.MostVisitedTilesProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Builds DropdownItemViewInfo list from AutocompleteResult for the Suggestions list. */
class DropdownItemViewInfoListBuilder {
    @Px
    private static final int DROPDOWN_HEIGHT_UNKNOWN = -1;
    private static final int DEFAULT_SIZE_OF_VISIBLE_GROUP = 5;

    private final @NonNull List<SuggestionProcessor> mPriorityOrderedSuggestionProcessors;
    private final @NonNull Supplier<Tab> mActivityTabSupplier;

    private @Nullable DividerLineProcessor mDividerLineProcessor;
    private @Nullable HeaderProcessor mHeaderProcessor;
    private @Nullable Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Nullable OmniboxImageSupplier mImageSupplier;
    private @NonNull BookmarkState mBookmarkState;
    @Px
    private int mDropdownHeight;
    private OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;

    DropdownItemViewInfoListBuilder(@NonNull Supplier<Tab> tabSupplier, BookmarkState bookmarkState,
            OpenHistoryClustersDelegate openHistoryClustersDelegate) {
        mPriorityOrderedSuggestionProcessors = new ArrayList<>();
        mDropdownHeight = DROPDOWN_HEIGHT_UNKNOWN;
        mActivityTabSupplier = tabSupplier;
        mBookmarkState = bookmarkState;
        mOpenHistoryClustersDelegate = openHistoryClustersDelegate;
    }

    /**
     * Initialize the Builder with default set of suggestion processors.
     *
     * @param context Current context.
     * @param host Component creating suggestion view delegates and responding to suggestion events.
     * @param textProvider Provider of querying/editing the Omnibox.
     */
    void initDefaultProcessors(
            Context context, SuggestionHost host, UrlBarEditingTextStateProvider textProvider) {
        assert mPriorityOrderedSuggestionProcessors.size() == 0 : "Processors already initialized.";

        final Supplier<ShareDelegate> shareSupplier =
                () -> mShareDelegateSupplier == null ? null : mShareDelegateSupplier.get();

        if (!OmniboxFeatures.isLowMemoryDevice()) {
            mImageSupplier = new OmniboxImageSupplier(context);
        }

        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                && !OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            // Only create DividerLineProcessor when feature is enabled.
            // Feature is enabled on non-tablet devices.
            mDividerLineProcessor = new DividerLineProcessor(context);
        }
        mHeaderProcessor = new HeaderProcessor(context);
        registerSuggestionProcessor(new EditUrlSuggestionProcessor(
                context, host, mImageSupplier, mActivityTabSupplier, shareSupplier));
        registerSuggestionProcessor(
                new AnswerSuggestionProcessor(context, host, textProvider, mImageSupplier));
        registerSuggestionProcessor(
                new ClipboardSuggestionProcessor(context, host, mImageSupplier));
        registerSuggestionProcessor(new HistoryClustersProcessor(mOpenHistoryClustersDelegate,
                context, host, textProvider, mImageSupplier, mBookmarkState));
        registerSuggestionProcessor(new EntitySuggestionProcessor(
                context, host, textProvider, mImageSupplier, mBookmarkState));
        registerSuggestionProcessor(new TailSuggestionProcessor(context, host));
        registerSuggestionProcessor(new MostVisitedTilesProcessor(context, host, mImageSupplier));
        registerSuggestionProcessor(new BasicSuggestionProcessor(
                context, host, textProvider, mImageSupplier, mBookmarkState));
    }

    void destroy() {
        if (mImageSupplier != null) {
            mImageSupplier.destroy();
            mImageSupplier = null;
        }
    }

    /**
     * Register new processor to process OmniboxSuggestions.
     * Processors will be tried in the same order as they were added.
     *
     * @param processor SuggestionProcessor that handles OmniboxSuggestions.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void registerSuggestionProcessor(SuggestionProcessor processor) {
        mPriorityOrderedSuggestionProcessors.add(processor);
    }

    /**
     * Specify instance of the HeaderProcessor that will be used to run tests.
     *
     * @param processor Header processor used to build suggestion headers.
     */
    void setHeaderProcessorForTest(HeaderProcessor processor) {
        mHeaderProcessor = processor;
    }

    /**
     * Specify instance of the DividerLineProcessor that will be used to run tests.
     *
     * @param processor divider line processor used to build the suggestion divider line.
     */
    void setDividerLineProcessorForTest(DividerLineProcessor processor) {
        mDividerLineProcessor = processor;
    }

    /**
     * Notify that the current User profile has changed.
     *
     * @param profile Current user profile.
     */
    void setProfile(Profile profile) {
        if (mImageSupplier != null) {
            mImageSupplier.setProfile(profile);
        }
    }

    /**
     * Notify that the current Share delegate supplier has changed.
     *
     * @param shareDelegateSupplier Share facility supplier.
     */
    void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    /**
     * Specify dropdown list height in pixels.
     * The height is subsequentially used to determine number of visible suggestions and perform
     * partial suggestion ordering based on their visibility.
     *
     * Note that this mechanism is effective as long as grouping is not in use in zero-prefix
     * context. At the time this mechanism was created, zero-prefix context never presented mixed
     * URL and (non-reactive) search suggestions, but instead presented either a list of specialized
     * suggestions (eg. clipboard, query tiles) mixed with reactive suggestions, a plain list of
     * search suggestions, or a plain list of recent URLs.
     * This gives us the chance to measure the height of the dropdown list before the actual
     * grouping takes effect.
     * If the above situation changes, we may need to revisit the logic here, and possibly cache the
     * heights in different states (eg. portrait mode, split screen etc) to get better results.
     *
     * @param dropdownHeight Updated height of the dropdown item list.
     */
    void setDropdownHeightWithKeyboardActive(@Px int dropdownHeight) {
        mDropdownHeight = dropdownHeight;
    }

    /**
     * Respond to omnibox session state change.
     *
     * @param activated Indicates whether omnibox session is activated.
     */
    void onOmniboxSessionStateChange(boolean activated) {
        if (!activated && mImageSupplier != null) mImageSupplier.resetCache();

        mHeaderProcessor.onOmniboxSessionStateChange(activated);
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onOmniboxSessionStateChange(activated);
        }
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mHeaderProcessor.onNativeInitialized();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onNativeInitialized();
        }
    }

    /**
     * Adaptive Suggestions logic: perform partial grouping by Search vs URL on the
     * AutocompleteResult.
     *
     * @param autocompleteResult the result to apply adaptive suggestions to
     */
    @VisibleForTesting
    void performPartialGroupingBySearchVsUrl(AutocompleteResult autocompleteResult) {
        // When Adaptive Suggestions are set, perform partial grouping by search vs url.
        // Take action only if we have more suggestions to offer than just a default match and
        // one suggestion (otherwise no need to perform grouping).
        if (autocompleteResult.getSuggestionsList().size() > 2) {
            final int firstSuggestionWithHeader =
                    getIndexOfFirstSuggestionWithHeader(autocompleteResult);
            final int numVisibleSuggestions = getVisibleSuggestionsCount(autocompleteResult);
            // TODO(crbug.com/1073169): this should either infer the count from UI height or supply
            // the default value if height is not known. For the time being we group the entire list
            // to mimic the native behavior.
            if (firstSuggestionWithHeader > 1) {
                autocompleteResult.groupSuggestionsBySearchVsURL(
                        1, Math.min(numVisibleSuggestions, firstSuggestionWithHeader));
            }
            if (numVisibleSuggestions < firstSuggestionWithHeader) {
                autocompleteResult.groupSuggestionsBySearchVsURL(
                        numVisibleSuggestions, firstSuggestionWithHeader);
            }
        }
    }

    /**
     * Create a vertical suggestions group ("section").
     *
     * The logic creates a vertically stacked set of suggestions that belong to the same Suggestions
     * Group ("section").
     * If the GroupConfig describing the group has a header text, it will be applied.
     * Each suggestion is permitted to be handled by a distinct, separate Processor.
     *
     * @param groupDetails the details describing this (vertical) suggestions group
     * @param groupMatches the matches that belong to this suggestions group
     * @param firstVerticalPosition the index of the first AutocompleteMatch in the target list
     */
    @VisibleForTesting
    @NonNull
    List<DropdownItemViewInfo> buildVerticalSuggestionsGroup(@NonNull GroupConfig groupDetails,
            @NonNull List<AutocompleteMatch> groupMatches, int firstVerticalPosition) {
        assert groupDetails != null;
        assert groupMatches != null;

        int numGroupMatches = groupMatches.size();
        assert numGroupMatches > 0;
        var result = new ArrayList<DropdownItemViewInfo>(numGroupMatches);

        // Only add the Header Group when both ID and details are specified.
        // Note that despite GroupsDetails map not holding <null> values,
        // a group definition for specific ID may be unavailable, or the group
        // header text may be empty.
        if (!TextUtils.isEmpty(groupDetails.getHeaderText())) {
            final PropertyModel model = mHeaderProcessor.createModel();
            mHeaderProcessor.populateModel(model, groupDetails.getHeaderText());
            result.add(new DropdownItemViewInfo(mHeaderProcessor, model, groupDetails));
        }

        for (int indexInList = 0; indexInList < numGroupMatches; indexInList++) {
            var indexOnList = firstVerticalPosition + indexInList;
            var match = groupMatches.get(indexInList);
            var processor = getProcessorForSuggestion(match, indexOnList);
            var model = processor.createModel();
            processor.populateModel(match, model, indexOnList);
            result.add(new DropdownItemViewInfo(processor, model, groupDetails));
        }

        return result;
    }

    /**
     * Create a horizontal suggestions group ("section").
     *
     * The logic creates a horizontally arranged set of suggestions that belong to the same
     * Suggestions Group ("section"). If the GroupConfig describing the group has a header text, it
     * will be applied. Each suggestion presently must be handled by the same processor.
     *
     * Once built, all the matches reported by this call are appended to the target list of
     * DropdownItemViewInfo objects, encompassing all suggestion groups.
     *
     * @param groupDetails the details describing this (vertical) suggestions group
     * @param groupMatches the matches that belong to this suggestions group
     * @param position the index on the target list
     */
    @VisibleForTesting
    @NonNull
    List<DropdownItemViewInfo> buildHorizontalSuggestionsGroup(@NonNull GroupConfig groupDetails,
            @NonNull List<AutocompleteMatch> groupMatches, int position) {
        assert groupDetails != null;
        assert groupMatches != null;
        assert groupMatches.size() > 0;

        var result = new ArrayList<DropdownItemViewInfo>();

        // Only add the Header Group when both ID and details are specified.
        // Note that despite GroupsDetails map not holding <null> values,
        // a group definition for specific ID may be unavailable, or the group
        // header text may be empty.
        if (!TextUtils.isEmpty(groupDetails.getHeaderText())) {
            final PropertyModel model = mHeaderProcessor.createModel();
            mHeaderProcessor.populateModel(model, groupDetails.getHeaderText());
            result.add(new DropdownItemViewInfo(mHeaderProcessor, model, groupDetails));
        }

        int numGroupMatches = groupMatches.size();
        var processor = getProcessorForSuggestion(groupMatches.get(0), position);
        var model = processor.createModel();

        for (int index = 0; index < numGroupMatches; index++) {
            var match = groupMatches.get(index);
            assert processor.doesProcessSuggestion(match, position);
            processor.populateModel(match, model, position);
        }

        result.add(new DropdownItemViewInfo(processor, model, groupDetails));
        return result;
    }

    /**
     * Build ListModel for new set of Omnibox suggestions.
     *
     * Collect suggestions by their Suggestion Group ("section"), and aggregate models for every
     * section in a resulting list of DropdownItemViewInfo.
     *
     * @param autocompleteResult New set of suggestions.
     * @return List of DropdownItemViewInfo representing the corresponding content of the
     *          suggestions list.
     */
    @NonNull
    List<DropdownItemViewInfo> buildDropdownViewInfoList(AutocompleteResult autocompleteResult) {
        mHeaderProcessor.onSuggestionsReceived();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onSuggestionsReceived();
        }

        performPartialGroupingBySearchVsUrl(autocompleteResult);

        var newMatches = autocompleteResult.getSuggestionsList();
        int newMatchesCount = newMatches.size();
        var viewInfoList = new ArrayList<DropdownItemViewInfo>();
        var currentGroupMatches = new ArrayList<AutocompleteMatch>();
        var nextSuggestionLogicalIndex = 0;

        // Add the divider line on top if the suggestion list is not empty.
        if (mDividerLineProcessor != null && newMatchesCount > 0) {
            final PropertyModel model = mDividerLineProcessor.createModel();
            viewInfoList.add(new DropdownItemViewInfo(mDividerLineProcessor, model, null));
        }

        // Outer loop to ensure suggestions are always added to the produced ViewInfo list.
        for (int index = 0; index < newMatchesCount;) {
            int currentGroupId = newMatches.get(index).getGroupId();
            currentGroupMatches.clear();

            // Inner loop to populate AutocompleteMatch objects belonging to this group.
            while (index < newMatchesCount) {
                var match = newMatches.get(index);
                if (currentGroupId != match.getGroupId()) break;
                currentGroupMatches.add(match);
                index++;
            }

            // Append this suggestions group/section to resulting model, following the render type
            // dictated by GroupConfig.
            // The default instance holds safe values, applicable to non-Google DSE.
            var currentGroupConfig = autocompleteResult.getGroupsInfo().getGroupConfigsOrDefault(
                    currentGroupId, GroupConfig.getDefaultInstance());
            if (currentGroupConfig.getRenderType() == GroupConfig.RenderType.DEFAULT_VERTICAL) {
                viewInfoList.addAll(buildVerticalSuggestionsGroup(
                        currentGroupConfig, currentGroupMatches, nextSuggestionLogicalIndex));
                nextSuggestionLogicalIndex += currentGroupMatches.size();
            } else if (currentGroupConfig.getRenderType() == GroupConfig.RenderType.HORIZONTAL) {
                viewInfoList.addAll(buildHorizontalSuggestionsGroup(
                        currentGroupConfig, currentGroupMatches, nextSuggestionLogicalIndex));
                // Only one suggestion added.
                nextSuggestionLogicalIndex++;
            } else {
                assert false : "Unsupported group render type: "
                               + currentGroupConfig.getRenderType();
            }
        }

        return viewInfoList;
    }

    /**
     * @param autocompleteResult The AutocompleteResult to analyze.
     * @return Number of suggestions immediately visible to the user upon presenting the list.
     *          Does not include the suggestions with headers, or VOICE_SUGGEST suggestions that
     *          have been injected by Java provider.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    int getVisibleSuggestionsCount(AutocompleteResult autocompleteResult) {
        // For cases where we don't know how many suggestions can fit in the visile screen area,
        // make an assumption regarding the group size.
        if (mDropdownHeight == DROPDOWN_HEIGHT_UNKNOWN) {
            return Math.min(
                    autocompleteResult.getSuggestionsList().size(), DEFAULT_SIZE_OF_VISIBLE_GROUP);
        }

        final List<AutocompleteMatch> suggestions = autocompleteResult.getSuggestionsList();

        @Px
        int calculatedSuggestionsHeight = 0;
        int lastVisibleIndex;
        for (lastVisibleIndex = 0; lastVisibleIndex < suggestions.size(); lastVisibleIndex++) {
            final AutocompleteMatch suggestion = suggestions.get(lastVisibleIndex);
            // We do not include suggestions with headers in partial grouping, so terminate early.
            if (suggestion.getGroupId() != AutocompleteMatch.INVALID_GROUP) {
                break;
            }

            final SuggestionProcessor processor =
                    getProcessorForSuggestion(suggestion, lastVisibleIndex);

            int itemHeight = processor.getMinimumViewHeight();

            // Evaluate suggestion and determine whether it should be considered visible or
            // concealed based on the degree to which it is exposed.
            // Suggestions exposed 50% or more (where at least half of the suggestion's height is
            // visible) are considered visible. Suggestions concealed 50% or more (more than half of
            // the usggestion's height is hidden) are considered fully concealed.
            if (calculatedSuggestionsHeight + (itemHeight / 2) <= mDropdownHeight) {
                // 50% or more of the content exposed.
                calculatedSuggestionsHeight += itemHeight;
            } else {
                break;
            }
        }

        return lastVisibleIndex;
    }

    /**
     * Returns the index of the first suggestion that has an associated group header ID.
     * - If no suggestions have group header ID set, returns the size of the list.
     * - If all suggestions have group header ID set, returns 0.
     */
    int getIndexOfFirstSuggestionWithHeader(AutocompleteResult autocompleteResult) {
        final List<AutocompleteMatch> suggestions = autocompleteResult.getSuggestionsList();
        // Suggestions with headers, if present, are always shown last. Iterate from the bottom of
        // the list to avoid scanning entire list when there are no headers.
        for (int suggestionIndex = suggestions.size() - 1; suggestionIndex >= 0;
                suggestionIndex--) {
            if (suggestions.get(suggestionIndex).getGroupId() == AutocompleteMatch.INVALID_GROUP) {
                return suggestionIndex + 1;
            }
        }
        return 0;
    }

    /**
     * Search for Processor that will handle the supplied suggestion at specific position.
     *
     * @param suggestion The suggestion to be processed.
     * @param position Position of the suggestion in the list.
     */
    private SuggestionProcessor getProcessorForSuggestion(
            AutocompleteMatch suggestion, int position) {
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            SuggestionProcessor processor = mPriorityOrderedSuggestionProcessors.get(index);
            if (processor.doesProcessSuggestion(suggestion, position)) return processor;
        }
        assert false : "No default handler for suggestions";
        return null;
    }
}
