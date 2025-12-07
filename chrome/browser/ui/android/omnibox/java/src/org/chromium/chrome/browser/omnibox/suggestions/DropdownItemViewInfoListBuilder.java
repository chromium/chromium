// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.groupseparator.GroupSeparatorProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.MostVisitedTilesProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tabgroup.TabGroupSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Builds DropdownItemViewInfo list from AutocompleteResult for the Suggestions list. */
@NullMarked
class DropdownItemViewInfoListBuilder {
    private final List<SuggestionProcessor> mPriorityOrderedSuggestionProcessors;
    private final Supplier<@Nullable Tab> mActivityTabSupplier;

    private GroupSeparatorProcessor mGroupSeparatorProcessor;
    private HeaderProcessor mHeaderProcessor;
    private @Nullable Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Nullable OmniboxImageSupplier mImageSupplier;
    private final BookmarkState mBookmarkState;
    private final ObservableSupplier<@ControlsPosition Integer> mToolbarPositionSupplier;

    DropdownItemViewInfoListBuilder(
            Supplier<@Nullable Tab> tabSupplier,
            BookmarkState bookmarkState,
            ObservableSupplier<@ControlsPosition Integer> toolbarPositionSupplier) {
        mPriorityOrderedSuggestionProcessors = new ArrayList<>();
        mActivityTabSupplier = tabSupplier;
        mImageSupplier = null;
        mBookmarkState = bookmarkState;
        mToolbarPositionSupplier = toolbarPositionSupplier;
    }

    /**
     * Creates a UI context object containing common dependencies for suggestion processors.
     *
     * @param context Current context
     * @param host Component creating suggestion view delegates and responding to suggestion events
     * @param textProvider Provider of querying/editing the Omnibox
     * @return AutocompleteUIContext with all necessary dependencies
     */
    private AutocompleteUIContext createUIContext(
            Context context, SuggestionHost host, UrlBarEditingTextStateProvider textProvider) {
        return new AutocompleteUIContext(
                context,
                host,
                textProvider,
                mImageSupplier,
                mBookmarkState,
                mActivityTabSupplier,
                mShareDelegateSupplier,
                mToolbarPositionSupplier);
    }

    /**
     * Initialize the Builder with default set of suggestion processors.
     *
     * @param context Current context.
     * @param host Component creating suggestion view delegates and responding to suggestion events.
     * @param textProvider Provider of querying/editing the Omnibox.
     */
    @Initializer
    void initDefaultProcessors(
            Context context, SuggestionHost host, UrlBarEditingTextStateProvider textProvider) {
        assert mPriorityOrderedSuggestionProcessors.size() == 0 : "Processors already initialized.";

        mImageSupplier =
                OmniboxFeatures.isLowMemoryDevice() ? null : new OmniboxImageSupplier(context);

        AutocompleteUIContext uiContext = createUIContext(context, host, textProvider);

        mGroupSeparatorProcessor = new GroupSeparatorProcessor(uiContext.context);
        mHeaderProcessor = new HeaderProcessor(uiContext.context);
        registerSuggestionProcessor(new EditUrlSuggestionProcessor(uiContext));
        registerSuggestionProcessor(new AnswerSuggestionProcessor(uiContext));
        registerSuggestionProcessor(new ClipboardSuggestionProcessor(uiContext));
        registerSuggestionProcessor(new EntitySuggestionProcessor(uiContext));
        registerSuggestionProcessor(new TailSuggestionProcessor(uiContext));
        registerSuggestionProcessor(new MostVisitedTilesProcessor(uiContext));
        if (OmniboxFeatures.sAndroidHubSearchTabGroups.isEnabled()) {
            registerSuggestionProcessor(new TabGroupSuggestionProcessor(uiContext));
        }
        registerSuggestionProcessor(new BasicSuggestionProcessor(uiContext));
    }

    void destroy() {
        if (mImageSupplier != null) {
            mImageSupplier.destroy();
        }
        mImageSupplier = null;
    }

    /**
     * Register new processor to process OmniboxSuggestions. Processors will be tried in the same
     * order as they were added.
     *
     * @param processor SuggestionProcessor that handles OmniboxSuggestions.
     */
    @VisibleForTesting
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
     * Specify instance of the GroupSeparatorProcessor that will be used to run tests.
     *
     * @param processor divider line processor used to build the suggestion divider line.
     */
    void setGroupSeparatorProcessorForTest(GroupSeparatorProcessor processor) {
        mGroupSeparatorProcessor = processor;
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
    void setShareDelegateSupplier(@Nullable Supplier<ShareDelegate> shareDelegateSupplier) {
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    /**
     * Respond to omnibox session state change.
     *
     * @param activated Indicates whether omnibox session is activated.
     */
    void onOmniboxSessionStateChange(boolean activated) {
        if (!activated && mImageSupplier != null) {
            mImageSupplier.resetCache();
        }

        mHeaderProcessor.onOmniboxSessionStateChange(activated);
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onOmniboxSessionStateChange(activated);
        }
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mHeaderProcessor.onNativeInitialized();
        if (mImageSupplier != null) {
            mImageSupplier.onNativeInitialized();
        }

        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onNativeInitialized();
        }
    }

    /**
     * Create a vertical suggestions group ("section").
     *
     * <p>The logic creates a vertically stacked set of suggestions that belong to the same
     * Suggestions Group ("section"). If the GroupConfig describing the group has a header text, it
     * will be applied. Each suggestion is permitted to be handled by a distinct, separate
     * Processor.
     *
     * @param input The input for which produced the suggestions.
     * @param groupDetails The details describing this (vertical) suggestions group
     * @param groupMatches The matches that belong to this suggestions group
     * @param firstVerticalPosition The index of the first AutocompleteMatch in the target list
     */
    @VisibleForTesting
    List<DropdownItemViewInfo> buildVerticalSuggestionsGroup(
            AutocompleteInput input,
            GroupConfig groupDetails,
            @Nullable GroupConfig previousDetails,
            List<AutocompleteMatch> groupMatches,
            int firstVerticalPosition) {
        assert groupDetails != null;
        assert groupMatches != null;

        int numGroupMatches = groupMatches.size();
        assert numGroupMatches > 0;
        var result = new ArrayList<DropdownItemViewInfo>(numGroupMatches);

        // Only add the Header Group when both ID and details are specified.
        // Note that despite GroupsDetails map not holding <null> values,
        // a group definition for specific ID may be unavailable, or the group
        // header text may be empty.
        // TODO(http://crbug/1518967): move this to the calling function and instantiate the
        // HeaderView undonditionally when passing from one suggestion group to another.
        // TODO(http://crbug/1518967): collapse Header and DivierLine to a single component.
        if (!TextUtils.isEmpty(groupDetails.getHeaderText())) {
            final PropertyModel model = mHeaderProcessor.createModel();
            mHeaderProcessor.populateModel(model, groupDetails.getHeaderText());
            result.add(new DropdownItemViewInfo(mHeaderProcessor, model, groupDetails));
        } else if (previousDetails != null
                && previousDetails.getRenderType() == GroupConfig.RenderType.DEFAULT_VERTICAL) {
            final PropertyModel model = mGroupSeparatorProcessor.createModel();
            result.add(new DropdownItemViewInfo(mGroupSeparatorProcessor, model, groupDetails));
        }

        boolean toolbarOnBottom =
                ChromeFeatureList.sAndroidBottomToolbarV2ReverseOrderSuggestionsList.getValue()
                        && assumeNonNull(mToolbarPositionSupplier.get()) == ControlsPosition.BOTTOM;
        var roundingStartEdge =
                toolbarOnBottom
                        ? DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED
                        : DropdownCommonProperties.BG_TOP_CORNER_ROUNDED;
        var roundingEndEdge =
                toolbarOnBottom
                        ? DropdownCommonProperties.BG_TOP_CORNER_ROUNDED
                        : DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED;

        for (int indexInList = 0; indexInList < numGroupMatches; indexInList++) {
            var indexOnList = firstVerticalPosition + indexInList;
            AutocompleteMatch match = groupMatches.get(indexInList);
            var processor = getProcessorForSuggestion(match, indexOnList);
            var model = processor.createModel();

            model.set(roundingStartEdge, indexInList == 0);
            model.set(roundingEndEdge, indexInList == numGroupMatches - 1);
            model.set(DropdownCommonProperties.SHOW_DIVIDER, indexInList < numGroupMatches - 1);

            processor.populateModel(input, match, model, indexOnList);
            result.add(new DropdownItemViewInfo(processor, model, groupDetails));
        }

        return result;
    }

    /**
     * Create a horizontal suggestions group ("section").
     *
     * <p>The logic creates a horizontally arranged set of suggestions that belong to the same
     * Suggestions Group ("section"). If the GroupConfig describing the group has a header text, it
     * will be applied. Each suggestion presently must be handled by the same processor.
     *
     * <p>Once built, all the matches reported by this call are appended to the target list of
     * DropdownItemViewInfo objects, encompassing all suggestion groups.
     *
     * @param input The input for which produced the suggestions.
     * @param groupDetails The details describing this (vertical) suggestions group
     * @param groupMatches The matches that belong to this suggestions group
     * @param position The index on the target list
     */
    @VisibleForTesting
    List<DropdownItemViewInfo> buildHorizontalSuggestionsGroup(
            AutocompleteInput input,
            GroupConfig groupDetails,
            List<AutocompleteMatch> groupMatches,
            int position) {
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
            AutocompleteMatch match = groupMatches.get(index);
            assert processor.doesProcessSuggestion(match, position);
            processor.populateModel(input, match, model, position);
        }

        result.add(new DropdownItemViewInfo(processor, model, groupDetails));
        return result;
    }

    /**
     * Build ListModel for new set of Omnibox suggestions.
     *
     * <p>Collect suggestions by their Suggestion Group ("section"), and aggregate models for every
     * section in a resulting list of DropdownItemViewInfo.
     *
     * @param input The input that generated the suggestions.
     * @param autocompleteResult New set of suggestions.
     * @return List of DropdownItemViewInfo representing the corresponding content of the
     *     suggestions list.
     */
    List<DropdownItemViewInfo> buildDropdownViewInfoList(
            AutocompleteInput input, AutocompleteResult autocompleteResult) {
        mHeaderProcessor.onSuggestionsReceived();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onSuggestionsReceived();
        }

        var newMatches = autocompleteResult.getSuggestionsList();
        int newMatchesCount = newMatches.size();
        var viewInfoList = new ArrayList<DropdownItemViewInfo>();
        var currentGroupMatches = new ArrayList<AutocompleteMatch>();
        var nextSuggestionLogicalIndex = 0;
        var groupsInfo = autocompleteResult.getGroupsInfo();

        GroupConfig previousGroupConfig = null;

        // Outer loop to ensure suggestions are always added to the produced ViewInfo list.
        for (int index = 0; index < newMatchesCount; ) {
            int currentGroupId = newMatches.get(index).getGroupId();
            currentGroupMatches.clear();

            var currentGroupConfig =
                    groupsInfo.getGroupConfigsOrDefault(
                            currentGroupId, GroupConfig.getDefaultInstance());

            // Inner loop to populate AutocompleteMatch objects belonging to this group.
            while (index < newMatchesCount) {
                var match = newMatches.get(index);
                if (OmniboxFeatures.sRemoveSroIncludingVerbatimMatch.getValue()) {
                    if (match.getType() == OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED
                            || match.getType() == OmniboxSuggestionType.URL_WHAT_YOU_TYPED) {
                        index++;
                        continue;
                    }
                }

                var matchGroupConfig =
                        groupsInfo.getGroupConfigsOrDefault(
                                match.getGroupId(), GroupConfig.getDefaultInstance());
                if (currentGroupConfig.getSection() != matchGroupConfig.getSection()) break;
                currentGroupMatches.add(match);
                index++;
            }

            // Corner case: if disabling SRO and the verbatim match on the Web, the top group may be
            // empty.
            if (currentGroupMatches.isEmpty()) continue;

            // Append this suggestions group/section to resulting model, following the render type
            // dictated by GroupConfig.
            // The default instance holds safe values, applicable to non-Google DSE.
            if (currentGroupConfig.getRenderType() == GroupConfig.RenderType.DEFAULT_VERTICAL) {
                viewInfoList.addAll(
                        buildVerticalSuggestionsGroup(
                                input,
                                currentGroupConfig,
                                previousGroupConfig,
                                currentGroupMatches,
                                nextSuggestionLogicalIndex));
                nextSuggestionLogicalIndex += currentGroupMatches.size();
            } else if (currentGroupConfig.getRenderType() == GroupConfig.RenderType.HORIZONTAL) {
                viewInfoList.addAll(
                        buildHorizontalSuggestionsGroup(
                                input,
                                currentGroupConfig,
                                currentGroupMatches,
                                nextSuggestionLogicalIndex));
                // Only one suggestion added.
                nextSuggestionLogicalIndex++;
            } else {
                assert false
                        : "Unsupported group render type: "
                                + currentGroupConfig.getRenderType().name();
            }

            previousGroupConfig = currentGroupConfig;
        }

        return viewInfoList;
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

        // Crash intentionally. This should never happen. The final suggestion processor is the
        // catch-all.
        throw new RuntimeException("No default handler for suggestions");
    }
}
