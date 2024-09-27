// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.history_clusters.HistoryClustersTabHelper;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Optional;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with the
 * rest of Chrome.
 */
public class EditUrlSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final @NonNull Supplier<ShareDelegate> mShareDelegateSupplier;
    private final @NonNull Supplier<Tab> mTabSupplier;

    public EditUrlSuggestionProcessor(
            Context context,
            SuggestionHost suggestionHost,
            Optional<OmniboxImageSupplier> imageSupplier,
            Supplier<Tab> tabSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        super(context, suggestionHost, imageSupplier);

        mTabSupplier = tabSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(@NonNull AutocompleteMatch suggestion, int position) {
        // The what-you-typed suggestion can potentially appear as the second suggestion in some
        // cases. If the first suggestion isn't the one we want, ignore all subsequent suggestions.
        if (position != 0) return false;

        // Fall back to the base suggestion processor when retaining omnibox on focus so as not to
        // show mobile-optimized actions in a desktop-like context.
        if (OmniboxFeatures.shouldRetainOmniboxOnFocus()) return false;

        Tab activeTab = mTabSupplier.get();
        if (activeTab == null
                || !activeTab.isInitialized()
                || activeTab.isNativePage()
                || SadTab.isShowing(activeTab)) {
            return false;
        }

        if (suggestion.getType() != OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                || !suggestion.getUrl().equals(activeTab.getUrl())) {
            return false;
        }

        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.EDIT_URL_SUGGESTION;
    }

    @Override
    public @NonNull PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);

        var tab = mTabSupplier.get();
        var title = suggestion.getDescription();
        if (!tab.isLoading()) {
            title = tab.getTitle();
        } else if (TextUtils.isEmpty(title)) {
            title = mContext.getResources().getText(R.string.tab_loading_default_title).toString();
        }

        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionSpannable(title));
        model.set(
                SuggestionViewProperties.TEXT_LINE_2_TEXT,
                new SuggestionSpannable(suggestion.getDisplayText()));

        setActionButtons(
                model,
                Arrays.asList(
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.ic_share_white_24dp, true),
                                OmniboxResourceProvider.getString(
                                        mContext, R.string.menu_share_page),
                                null,
                                this::onShareLink),
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.ic_content_copy_black, true),
                                OmniboxResourceProvider.getString(mContext, R.string.copy_link),
                                () -> onCopyLink(suggestion)),
                        // TODO(crbug.com/40697047): do not re-use bookmark_item_edit here.
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.bookmark_edit_active, true),
                                OmniboxResourceProvider.getString(
                                        mContext, R.string.bookmark_item_edit),
                                () -> onEditLink(suggestion))));

        fetchSuggestionFavicon(model, suggestion.getUrl());
    }

    @Override
    protected void onSuggestionClicked(@NonNull AutocompleteMatch suggestion, int position) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Tap");
        super.onSuggestionClicked(suggestion, position);
    }

    /** Invoked when user interacts with Share action button. */
    private void onShareLink() {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Share");
        var webContents = mTabSupplier.get().getWebContents();
        if (webContents != null) {
            // TODO(ender): find out if this is still captured anywhere.
            new UkmRecorder.Bridge()
                    .recordEventWithBooleanMetric(
                            webContents, "Omnibox.EditUrlSuggestion.Share", "HasOccurred");
        }
        mSuggestionHost.finishInteraction();
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
        mSuggestionHost.onRefineSuggestion(suggestion);
    }
}
