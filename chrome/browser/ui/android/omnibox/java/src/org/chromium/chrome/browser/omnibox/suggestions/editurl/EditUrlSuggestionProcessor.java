// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history_clusters.HistoryClustersTabHelper;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.function.Supplier;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with the
 * rest of Chrome.
 */
@NullMarked
public class EditUrlSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final @Nullable Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<@Nullable Tab> mTabSupplier;

    /**
     * @param uiContext Context object containing common UI dependencies.
     */
    public EditUrlSuggestionProcessor(AutocompleteUIContext uiContext) {
        super(uiContext);
        mTabSupplier = uiContext.activityTabSupplier;
        mShareDelegateSupplier = uiContext.shareDelegateSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        // The what-you-typed suggestion can potentially appear as the second suggestion in some
        // cases. If the first suggestion isn't the one we want, ignore all subsequent suggestions.
        if (position != 0) return false;

        if (OmniboxFeatures.sRemoveSearchReadyOmnibox.isEnabled()) return false;

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

        if ((suggestion.getType() != OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                        && suggestion.getType() != OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
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
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        super.populateModel(input, suggestion, model, position);

        var tab = mTabSupplier.get();
        assumeNonNull(tab);
        var title = suggestion.getDescription();
        if (!tab.isLoading()) {
            title = tab.getTitle();
        } else if (TextUtils.isEmpty(title)) {
            title = mContext.getResources().getText(R.string.tab_loading_default_title).toString();
        }

        boolean isSearch = suggestion.isSearchSuggestion();
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionSpannable(title));

        model.set(
                SuggestionViewProperties.TEXT_LINE_2_TEXT,
                isSearch ? null : new SuggestionSpannable(suggestion.getDisplayText()));

        String pageTitle = isSearch ? suggestion.getDisplayText() : suggestion.getDescription();
        String pageDomain = suggestion.getUrl().getHost();
        if (pageDomain.startsWith("www.")) {
            pageDomain = pageDomain.substring(4);
        }

        setActionButtons(
                model,
                Arrays.asList(
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.ic_share_white_24dp, true),
                                OmniboxResourceProvider.getString(
                                        mContext,
                                        isSearch
                                                ? R.string.accessibility_omnibox_btn_share_srp
                                                : R.string.accessibility_omnibox_btn_share_url,
                                        pageTitle,
                                        pageDomain),
                                null,
                                this::onShareLink),
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.ic_content_copy, true),
                                OmniboxResourceProvider.getString(
                                        mContext,
                                        isSearch
                                                ? R.string.accessibility_omnibox_btn_copy_srp
                                                : R.string.accessibility_omnibox_btn_copy_url,
                                        pageTitle,
                                        pageDomain),
                                () -> onCopyLink(suggestion)),
                        new Action(
                                OmniboxDrawableState.forSmallIcon(
                                        mContext, R.drawable.bookmark_edit_active, true),
                                OmniboxResourceProvider.getString(
                                        mContext,
                                        isSearch
                                                ? R.string.accessibility_omnibox_btn_edit_query
                                                : R.string.accessibility_omnibox_btn_edit_url,
                                        isSearch ? pageTitle : pageDomain),
                                () -> onEditLink(suggestion))));

        fetchSuggestionFavicon(model, suggestion.getUrl());
    }

    @Override
    protected void onSuggestionClicked(AutocompleteMatch suggestion, int position) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Tap");
        super.onSuggestionClicked(suggestion, position);
    }

    /** Invoked when user interacts with Share action button. */
    private void onShareLink() {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Share");
        Tab tab = assumeNonNull(mTabSupplier.get());
        var webContents = tab.getWebContents();
        if (webContents != null) {
            // TODO(ender): find out if this is still captured anywhere.
            new UkmRecorder(webContents, "Omnibox.EditUrlSuggestion.Share")
                    .addBooleanMetric("HasOccurred")
                    .record();
        }
        mSuggestionHost.finishInteraction();
        // TODO(mdjones): This should only share the displayed URL instead of the background tab.
        assumeNonNull(mShareDelegateSupplier);
        mShareDelegateSupplier.get().share(tab, false, ShareOrigin.EDIT_URL);
    }

    /** Invoked when user interacts with Copy action button. */
    private void onCopyLink(AutocompleteMatch suggestion) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Copy");
        Tab tab = assumeNonNull(mTabSupplier.get());
        HistoryClustersTabHelper.onCurrentTabUrlCopied(tab.getWebContents());
        GURL cleanUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(suggestion.getUrl());
        Clipboard.getInstance().copyUrlToClipboard(cleanUrl);
    }

    /** Invoked when user interacts with Edit action button. */
    private void onEditLink(AutocompleteMatch suggestion) {
        RecordUserAction.record("Omnibox.EditUrlSuggestion.Edit");
        mSuggestionHost.onRefineSuggestion(suggestion);
    }
}
