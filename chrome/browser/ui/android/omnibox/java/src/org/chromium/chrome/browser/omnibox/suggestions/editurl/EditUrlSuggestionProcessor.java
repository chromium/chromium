// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.history_clusters.HistoryClustersTabHelper;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with
 * the rest of Chrome.
 */
public class EditUrlSuggestionProcessor extends BaseSuggestionViewProcessor {
    /** The delegate for accessing the location bar for observation and modification. */
    private final UrlBarDelegate mUrlBarDelegate;

    /** The delegate for accessing the sharing feature. */
    private final Supplier<ShareDelegate> mShareDelegateSupplier;

    /** A means of accessing the activity's tab. */
    private final Supplier<Tab> mTabSupplier;

    /** Whether the omnibox has already cleared its content for the focus event. */
    private boolean mHasClearedOmniboxForFocus;

    /**
     * @param locationBarDelegate A means of modifying the location bar.
     */
    public EditUrlSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarDelegate locationBarDelegate, FaviconFetcher faviconFetcher,
            Supplier<Tab> tabSupplier, Supplier<ShareDelegate> shareDelegateSupplier) {
        super(context, suggestionHost, faviconFetcher);

        mUrlBarDelegate = locationBarDelegate;
        mTabSupplier = tabSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        // The what-you-typed suggestion can potentially appear as the second suggestion in some
        // cases. If the first suggestion isn't the one we want, ignore all subsequent suggestions.
        if (position != 0) return false;

        Tab activeTab = mTabSupplier.get();
        if (activeTab == null || !activeTab.isInitialized() || activeTab.isNativePage()
                || SadTab.isShowing(activeTab)) {
            return false;
        }

        if (suggestion.getType() != OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                || !suggestion.getUrl().equals(activeTab.getUrl())) {
            return false;
        }

        if (!mHasClearedOmniboxForFocus && mUrlBarDelegate.shouldClearOmniboxOnFocus()) {
            mHasClearedOmniboxForFocus = true;
            mUrlBarDelegate.setOmniboxEditingText("");
        }
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.EDIT_URL_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);

        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionSpannable(mTabSupplier.get().getTitle()));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT,
                new SuggestionSpannable(suggestion.getDisplayText()));

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forDrawableRes(mContext, R.drawable.ic_globe_24dp)
                        .setAllowTint(true)
                        .build());

        setActionButtons(model,
                Arrays.asList(
                        new Action(SuggestionDrawableState.Builder
                                           .forDrawableRes(mContext, R.drawable.ic_share_white_24dp)
                                           .setLarge(true)
                                           .setAllowTint(true)
                                           .build(),
                                OmniboxResourceProvider.getString(
                                        mContext, R.string.menu_share_page),
                                null, this::onShareLink),
                        new Action(
                                SuggestionDrawableState.Builder
                                        .forDrawableRes(mContext, R.drawable.ic_content_copy_black)
                                        .setLarge(true)
                                        .setAllowTint(true)
                                        .build(),
                                OmniboxResourceProvider.getString(mContext, R.string.copy_link),
                                () -> onCopyLink(suggestion)),
                        // TODO(https://crbug.com/1090187): do not re-use bookmark_item_edit here.
                        new Action(
                                SuggestionDrawableState.Builder
                                        .forDrawableRes(mContext, R.drawable.bookmark_edit_active)
                                        .setLarge(true)
                                        .setAllowTint(true)
                                        .build(),
                                OmniboxResourceProvider.getString(
                                        mContext, R.string.bookmark_item_edit),
                                () -> onEditLink(suggestion))));

        fetchSuggestionFavicon(model, suggestion.getUrl());
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);
        if (hasFocus) return;
        mHasClearedOmniboxForFocus = false;
    }

    @Override
    protected void onSuggestionClicked(AutocompleteMatch suggestion, int position) {
        super.onSuggestionClicked(suggestion, position);
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Tap");
    }

    /** Invoked when user interacts with Share action button. */
    private void onShareLink() {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Share");
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.getWebContents() != null) {
            new UkmRecorder.Bridge().recordEventWithBooleanMetric(
                    mTabSupplier.get().getWebContents(), "Omnibox.EditUrlSuggestion.Share",
                    "HasOccurred");
        }
        mUrlBarDelegate.clearOmniboxFocus();
        // TODO(mdjones): This should only share the displayed URL instead of the background tab.
        mShareDelegateSupplier.get().share(mTabSupplier.get(), false, ShareOrigin.EDIT_URL);
    }

    /** Invoked when user interacts with Copy action button. */
    private void onCopyLink(AutocompleteMatch suggestion) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Copy");
        HistoryClustersTabHelper.onCurrentTabUrlCopied(mTabSupplier.get().getWebContents());
        Clipboard.getInstance().copyUrlToClipboard(suggestion.getUrl());
    }

    /** Invoked when user interacts with Edit action button. */
    private void onEditLink(AutocompleteMatch suggestion) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Edit");
        mUrlBarDelegate.setOmniboxEditingText(suggestion.getUrl().getSpec());
    }
}
