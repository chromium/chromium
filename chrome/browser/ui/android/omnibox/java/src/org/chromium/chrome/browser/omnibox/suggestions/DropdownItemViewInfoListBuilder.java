// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.HistoryClustersProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.dividerline.DividerLineProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.MostVisitedTilesProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Builds DropdownItemViewInfo list from AutocompleteResult for the Suggestions list. */
class DropdownItemViewInfoListBuilder {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;
    @Px
    private static final int DROPDOWN_HEIGHT_UNKNOWN = -1;
    private static final int DEFAULT_SIZE_OF_VISIBLE_GROUP = 5;

    private final @NonNull List<SuggestionProcessor> mPriorityOrderedSuggestionProcessors;
    private final @NonNull Supplier<Tab> mActivityTabSupplier;
    private final @NonNull ActionChipsDelegate mActionChipsDelegate;

    private @Nullable DividerLineProcessor mDividerLineProcessor;
    private @Nullable HeaderProcessor mHeaderProcessor;
    private @Nullable Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Nullable ImageFetcher mImageFetcher;
    private @Nullable FaviconFetcher mFaviconFetcher;
    private @Nullable LargeIconBridge mIconBridge;
    private @NonNull BookmarkState mBookmarkState;
    @Px
    private int mDropdownHeight;
    private OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;

    DropdownItemViewInfoListBuilder(@NonNull Supplier<Tab> tabSupplier, BookmarkState bookmarkState,
            @NonNull ActionChipsDelegate actionChipsDelegate,
            OpenHistoryClustersDelegate openHistoryClustersDelegate) {
        mPriorityOrderedSuggestionProcessors = new ArrayList<>();
        mDropdownHeight = DROPDOWN_HEIGHT_UNKNOWN;
        mActivityTabSupplier = tabSupplier;
        mBookmarkState = bookmarkState;
        mActionChipsDelegate = actionChipsDelegate;
        mOpenHistoryClustersDelegate = openHistoryClustersDelegate;
    }

    /**
     * Initialize the Builder with default set of suggestion processors.
     *
     * @param context Current context.
     * @param host Component creating suggestion view delegates and responding to suggestion events.
     * @param delegate Component facilitating interactions with UI and Autocomplete mechanism.
     * @param textProvider Provider of querying/editing the Omnibox.
     */
    void initDefaultProcessors(Context context, SuggestionHost host, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider) {
        assert mPriorityOrderedSuggestionProcessors.size() == 0 : "Processors already initialized.";

        final Supplier<ImageFetcher> imageFetcherSupplier = () -> mImageFetcher;
        final Supplier<LargeIconBridge> iconBridgeSupplier = () -> mIconBridge;
        final Supplier<ShareDelegate> shareSupplier =
                () -> mShareDelegateSupplier == null ? null : mShareDelegateSupplier.get();

        mFaviconFetcher = new FaviconFetcher(context, iconBridgeSupplier);

        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                && !OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            // Only create DividerLineProcessor when feature is enabled.
            // Feature is enabled on non-tablet devices.
            mDividerLineProcessor = new DividerLineProcessor(context);
        }
        mHeaderProcessor = new HeaderProcessor(context);
        registerSuggestionProcessor(new EditUrlSuggestionProcessor(
                context, host, delegate, mFaviconFetcher, mActivityTabSupplier, shareSupplier));
        registerSuggestionProcessor(new AnswerSuggestionProcessor(
                context, host, mActionChipsDelegate, textProvider, imageFetcherSupplier));
        registerSuggestionProcessor(
                new ClipboardSuggestionProcessor(context, host, mFaviconFetcher));
        registerSuggestionProcessor(new HistoryClustersProcessor(mOpenHistoryClustersDelegate,
                context, host, textProvider, mFaviconFetcher, mBookmarkState));
        registerSuggestionProcessor(new EntitySuggestionProcessor(
                context, host, mActionChipsDelegate, imageFetcherSupplier));
        registerSuggestionProcessor(
                new TailSuggestionProcessor(context, host, mActionChipsDelegate));
        registerSuggestionProcessor(new MostVisitedTilesProcessor(context, host, mFaviconFetcher));
        registerSuggestionProcessor(new BasicSuggestionProcessor(context, host,
                mActionChipsDelegate, textProvider, mFaviconFetcher, mBookmarkState));
    }

    void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
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
        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mFaviconFetcher != null) {
            mFaviconFetcher.clearCache();
        }

        mIconBridge = new LargeIconBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_ONLY,
                profile.getProfileKey(), GlobalDiscardableReferencePool.getReferencePool(),
                MAX_IMAGE_CACHE_SIZE);
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
     * Respond to URL bar focus change.
     *
     * @param hasFocus Indicates whether URL bar is now focused.
     */
    void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            if (mImageFetcher != null) mImageFetcher.clear();
            if (mFaviconFetcher != null) mFaviconFetcher.clearCache();
        }

        mHeaderProcessor.onUrlFocusChange(hasFocus);
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onUrlFocusChange(hasFocus);
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
     * Build ListModel for new set of Omnibox suggestions.
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

        final int suggestionsCount = autocompleteResult.getSuggestionsList().size();
        var groupConfigs = autocompleteResult.getGroupsInfo().getGroupConfigsMap();

        // When Adaptive Suggestions are set, perform partial grouping by search vs url.
        // Take action only if we have more suggestions to offer than just a default match and
        // one suggestion (otherwise no need to perform grouping).
        if (suggestionsCount > 2) {
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

        final List<AutocompleteMatch> newSuggestions = autocompleteResult.getSuggestionsList();
        final int newSuggestionsCount = newSuggestions.size();
        final List<DropdownItemViewInfo> viewInfoList = new ArrayList<>();

        // Match suggestions with their corresponding processors.
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors =
                new ArrayList<>();
        for (int index = 0; index < newSuggestionsCount; index++) {
            final AutocompleteMatch suggestion = newSuggestions.get(index);
            final SuggestionProcessor processor = getProcessorForSuggestion(suggestion, index);
            suggestionsPairedWithProcessors.add(new Pair<>(suggestion, processor));
        }

        // Build ViewInfo structures.
        int currentGroup = AutocompleteMatch.INVALID_GROUP;

        // Add the divider line on top if the suggestion list is not empty.
        if (mDividerLineProcessor != null && newSuggestionsCount > 0) {
            final PropertyModel model = mDividerLineProcessor.createModel();
            viewInfoList.add(new DropdownItemViewInfo(mDividerLineProcessor, model, currentGroup));
        }
        for (int index = 0; index < newSuggestionsCount; index++) {
            final Pair<AutocompleteMatch, SuggestionProcessor> suggestionAndProcessorPair =
                    suggestionsPairedWithProcessors.get(index);
            final AutocompleteMatch suggestion = suggestionAndProcessorPair.first;
            final SuggestionProcessor processor = suggestionAndProcessorPair.second;

            // Note: with suggestion grouping in place, the condition below also
            // determines rounding boundaries of suggestion group.
            if (currentGroup != suggestion.getGroupId()) {
                currentGroup = suggestion.getGroupId();
                final var details = groupConfigs.get(currentGroup);

                // Only add the Header Group when both ID and details are specified.
                // Note that despite GroupsDetails map not holding <null> values,
                // a group definition for specific ID may be unavailable, or the group
                // header text may be empty.
                if (details != null && !TextUtils.isEmpty(details.getHeaderText())) {
                    final PropertyModel model = mHeaderProcessor.createModel();
                    mHeaderProcessor.populateModel(model, details.getHeaderText());
                    viewInfoList.add(
                            new DropdownItemViewInfo(mHeaderProcessor, model, currentGroup));
                }
            }

            final PropertyModel model = processor.createModel();
            processor.populateModel(suggestion, model, index);
            viewInfoList.add(new DropdownItemViewInfo(processor, model, currentGroup));
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
        final boolean useOldEligibilityLogic =
                !OmniboxFeatures.adaptiveSuggestionsVisibleGroupEligibilityUpdate();

        @Px
        int calculatedSuggestionsHeight = 0;
        int lastVisibleIndex;
        for (lastVisibleIndex = 0; lastVisibleIndex < suggestions.size(); lastVisibleIndex++) {
            if (useOldEligibilityLogic && (calculatedSuggestionsHeight >= mDropdownHeight)) break;

            final AutocompleteMatch suggestion = suggestions.get(lastVisibleIndex);
            // We do not include suggestions with headers in partial grouping, so terminate early.
            if (suggestion.getGroupId() != AutocompleteMatch.INVALID_GROUP) {
                break;
            }

            final SuggestionProcessor processor =
                    getProcessorForSuggestion(suggestion, lastVisibleIndex);

            int itemHeight = processor.getMinimumViewHeight();

            if (useOldEligibilityLogic) {
                calculatedSuggestionsHeight += itemHeight;
                continue;
            }

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
