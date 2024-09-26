// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.MatchClassification;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Optional;

/** A class that handles base properties and model for most suggestions. */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    protected final @NonNull Context mContext;
    protected final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull ActionChipsProcessor mActionChipsProcessor;
    private final @NonNull Optional<OmniboxImageSupplier> mImageSupplier;
    private final int mDesiredFaviconWidthPx;
    private final int mDecorationImageSizePx;
    private final int mSuggestionSizePx;

    /**
     * @param context Current context.
     * @param host A handle to the object using the suggestions.
     * @param imageSupplier A mechanism to use to retrieve favicons.
     */
    public BaseSuggestionViewProcessor(
            @NonNull Context context,
            @NonNull SuggestionHost host,
            @NonNull Optional<OmniboxImageSupplier> imageSupplier) {
        mContext = context;
        mSuggestionHost = host;
        mImageSupplier = imageSupplier;
        mDesiredFaviconWidthPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);
        mDecorationImageSizePx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_decoration_image_size);
        mSuggestionSizePx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
        mActionChipsProcessor = new ActionChipsProcessor(host);
    }

    /**
     * @return The desired size of Omnibox suggestion favicon.
     */
    protected int getDesiredFaviconSize() {
        return mDesiredFaviconWidthPx;
    }

    /**
     * @return The size of suggestion decoration images in pixels.
     */
    protected int getDecorationImageSize() {
        return mDecorationImageSizePx;
    }

    /** Return whether this suggestion can host OmniboxAction chips. */
    protected boolean allowOmniboxActions() {
        return true;
    }

    @Override
    public int getMinimumViewHeight() {
        return mSuggestionSizePx;
    }

    /**
     * Retrieve fallback icon for a given suggestion. Must be completed synchromously.
     *
     * @param match AutocompleteMatch instance to retrieve fallback icon for
     * @return OmniboxDrawableState that can be immediately applied to suggestion view
     */
    protected @NonNull OmniboxDrawableState getFallbackIcon(@NonNull AutocompleteMatch match) {
        int icon =
                match.isSearchSuggestion()
                        ? R.drawable.ic_suggestion_magnifier
                        : R.drawable.ic_globe_24dp;
        return OmniboxDrawableState.forSmallIcon(mContext, icon, true);
    }

    /**
     * Specify OmniboxDrawableState for suggestion decoration.
     *
     * @param model the PropertyModel to apply the decoration to
     * @param decoration the OmniboxDrawableState to apply
     */
    protected void setOmniboxDrawableState(
            @NonNull PropertyModel model, @NonNull OmniboxDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify OmniboxDrawableState for action button.
     *
     * @param model Property model to update.
     * @param actions List of actions for the suggestion.
     */
    protected void setActionButtons(@NonNull PropertyModel model, @Nullable List<Action> actions) {
        model.set(BaseSuggestionViewProperties.ACTION_BUTTONS, actions);
    }

    /**
     * Setup action icon base on the suggestion, either show query build arrow or switch to tab.
     *
     * @param model Property model to update.
     * @param suggestion Suggestion associated with the action button.
     * @param position The position of the button in the list.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setTabSwitchOrRefineAction(
            @NonNull PropertyModel model, @NonNull AutocompleteMatch suggestion, int position) {
        @DrawableRes int icon;
        String iconString;
        Runnable action;
        if (suggestion.hasTabMatch()) {
            icon = R.drawable.switch_to_tab;
            iconString =
                    OmniboxResourceProvider.getString(
                            mContext, R.string.accessibility_omnibox_switch_to_tab);
            action = () -> mSuggestionHost.onSwitchToTab(suggestion, position);
        } else {
            iconString =
                    OmniboxResourceProvider.getString(
                            mContext,
                            R.string.accessibility_omnibox_btn_refine,
                            suggestion.getFillIntoEdit());
            icon = R.drawable.btn_suggestion_refine;
            action =
                    () -> {
                        if (suggestion.isSearchSuggestion()) {
                            RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
                        } else {
                            RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
                        }
                        mSuggestionHost.onRefineSuggestion(suggestion);
                    };
        }
        setActionButtons(
                model,
                List.of(
                        new Action(
                                OmniboxDrawableState.forSmallIcon(mContext, icon, true),
                                iconString,
                                action)));
    }

    /**
     * Process the click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionClicked(@NonNull AutocompleteMatch suggestion, int position) {
        mSuggestionHost.onSuggestionClicked(suggestion, position, suggestion.getUrl());
    }

    /**
     * Process the long-click event.
     *
     * @param suggestion Selected suggestion.
     */
    protected void onSuggestionLongClicked(@NonNull AutocompleteMatch suggestion) {
        mSuggestionHost.onDeleteMatch(suggestion, suggestion.getDisplayText());
    }

    /**
     * Process the touch down event. Only handles search suggestions.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggesiton on the list.
     */
    protected void onSuggestionTouchDownEvent(@NonNull AutocompleteMatch suggestion, int position) {
        try (TimingMetric metric = OmniboxMetrics.recordTouchDownProcessTime()) {
            mSuggestionHost.onSuggestionTouchDown(suggestion, position);
        }
    }

    @Override
    public void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position) {
        model.set(
                BaseSuggestionViewProperties.ON_CLICK,
                () -> onSuggestionClicked(suggestion, position));
        model.set(
                BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onSuggestionLongClicked(suggestion));
        model.set(
                BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION,
                () -> mSuggestionHost.setOmniboxEditingText(suggestion.getFillIntoEdit()));
        setActionButtons(model, null);

        model.set(BaseSuggestionViewProperties.USE_LARGE_DECORATION, false);
        model.set(BaseSuggestionViewProperties.SHOW_DECORATION, true);
        model.set(
                BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING,
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));
        model.set(BaseSuggestionViewProperties.TOP_PADDING, 0);

        if (OmniboxFeatures.isTouchDownTriggerForPrefetchEnabled()
                && !OmniboxFeatures.isLowMemoryDevice()
                && suggestion.isSearchSuggestion()) {
            model.set(
                    BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT,
                    () -> onSuggestionTouchDownEvent(suggestion, position));
        }

        if (allowOmniboxActions()) {
            mActionChipsProcessor.populateModel(suggestion, model, position);
        }

        var icon = getFallbackIcon(suggestion);
        assert icon != null;
        setOmniboxDrawableState(model, icon);
        if (suggestion.isSearchSuggestion()) {
            fetchImage(model, suggestion.getImageUrl());
        }
    }

    @Override
    @CallSuper
    public void onOmniboxSessionStateChange(boolean activated) {
        mActionChipsProcessor.onOmniboxSessionStateChange(activated);
    }

    @Override
    @CallSuper
    public void onSuggestionsReceived() {
        mActionChipsProcessor.onSuggestionsReceived();
    }

    /**
     * Apply In-Place highlight to matching sections of Suggestion text.
     *
     * @param text Suggestion text to apply highlight to.
     * @param classifications Classifications describing how to format text.
     * @return true, if at least one highlighted match section was found.
     */
    protected static boolean applyHighlightToMatchRegions(
            Spannable text, List<MatchClassification> classifications) {
        if (text == null || classifications == null) return false;

        boolean hasAtLeastOneMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = text.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, text.length());
                matchEndIndex = Math.min(matchEndIndex, text.length());

                hasAtLeastOneMatch = true;
                // Bold the part of the URL that matches the user query.
                text.setSpan(
                        new StyleSpan(Typeface.BOLD),
                        matchStartIndex,
                        matchEndIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasAtLeastOneMatch;
    }

    /**
     * Fetch suggestion favicon, if one is available. Updates icon decoration in supplied |model| if
     * |url| is not null and points to an already visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     */
    protected void fetchSuggestionFavicon(@NonNull PropertyModel model, @NonNull GURL url) {
        mImageSupplier.ifPresent(
                s ->
                        s.fetchFavicon(
                                url,
                                icon -> {
                                    if (icon != null) {
                                        setOmniboxDrawableState(
                                                model,
                                                OmniboxDrawableState.forFavIcon(mContext, icon));
                                    }
                                }));
    }

    /**
     * Fetch suggestion image. Updates icon decoration in supplied |model| if |imageUrl| is valid,
     * points to an image, and was successfully retrieved and decompressed.
     *
     * @param model the PropertyModel to update with retrieved image
     * @param imageUrl the URL of the image to retrieve and decode
     */
    protected void fetchImage(@NonNull PropertyModel model, @NonNull GURL imageUrl) {
        mImageSupplier.ifPresent(
                s ->
                        s.fetchImage(
                                imageUrl,
                                bitmap -> {
                                    if (bitmap != null) {
                                        setOmniboxDrawableState(
                                                model,
                                                OmniboxDrawableState.forImage(mContext, bitmap));
                                    }
                                }));
    }
}
