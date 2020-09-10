// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 *  Provides embedder-level information to PageInfoController.
 */
public abstract class PageInfoControllerDelegate {
    @IntDef({OfflinePageState.NOT_OFFLINE_PAGE, OfflinePageState.TRUSTED_OFFLINE_PAGE,
            OfflinePageState.UNTRUSTED_OFFLINE_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OfflinePageState {
        int NOT_OFFLINE_PAGE = 1;
        int TRUSTED_OFFLINE_PAGE = 2;
        int UNTRUSTED_OFFLINE_PAGE = 3;
    }

    @IntDef({PreviewPageState.NOT_PREVIEW, PreviewPageState.SECURE_PAGE_PREVIEW,
            PreviewPageState.INSECURE_PAGE_PREVIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreviewPageState {
        int NOT_PREVIEW = 1;
        int SECURE_PAGE_PREVIEW = 2;
        int INSECURE_PAGE_PREVIEW = 3;
    }

    private final Supplier<ModalDialogManager> mModalDialogManager;
    private final AutocompleteSchemeClassifier mAutocompleteSchemeClassifier;
    private final VrHandler mVrHandler;
    private final boolean mIsSiteSettingsAvailable;
    private final boolean mCookieControlsShown;
    protected @PreviewPageState int mPreviewPageState;
    protected @OfflinePageState int mOfflinePageState;
    protected boolean mIsHttpsImageCompressionApplied;
    protected String mOfflinePageUrl;

    public PageInfoControllerDelegate(Supplier<ModalDialogManager> modalDialogManager,
            AutocompleteSchemeClassifier autocompleteSchemeClassifier, VrHandler vrHandler,
            boolean isSiteSettingsAvailable, boolean cookieControlsShown) {
        mModalDialogManager = modalDialogManager;
        mAutocompleteSchemeClassifier = autocompleteSchemeClassifier;
        mVrHandler = vrHandler;
        mIsSiteSettingsAvailable = isSiteSettingsAvailable;
        mCookieControlsShown = cookieControlsShown;
        mIsHttpsImageCompressionApplied = false;

        // These sometimes get overwritten by derived classes.
        mPreviewPageState = PreviewPageState.NOT_PREVIEW;
        mOfflinePageState = OfflinePageState.NOT_OFFLINE_PAGE;
        mOfflinePageUrl = null;
    }
    /**
     * Creates an AutoCompleteClassifier.
     */
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return mAutocompleteSchemeClassifier;
    }

    /**
     * Whether cookie controls should be shown in Page Info UI.
     */
    public boolean cookieControlsShown() {
        return mCookieControlsShown;
    }

    /**
     * Return the ModalDialogManager to be used.
     */
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManager.get();
    }

    /**
     * Initialize viewParams with Preview UI info, if any.
     * @param viewParams The params to be initialized with Preview UI info.
     * @param runAfterDismiss Used to set "show original" callback on Previews UI.
     */
    public void initPreviewUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        // Don't support Preview UI by default.
        viewParams.previewUIShown = false;
        viewParams.previewSeparatorShown = false;
    }

    /**
     * Whether website dialog is displayed for a preview.
     */
    public boolean isShowingPreview() {
        return mPreviewPageState != PreviewPageState.NOT_PREVIEW;
    }

    /**
     * Whether Preview page state is INSECURE.
     */
    public boolean isPreviewPageInsecure() {
        return mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW;
    }

    /**
     * Returns whether or not an instant app is available for |url|.
     */
    public boolean isInstantAppAvailable(String url) {
        return false;
    }

    /**
     * Returns whether LiteMode https image compression was applied on this page
     */
    public boolean isHttpsImageCompressionApplied() {
        return mIsHttpsImageCompressionApplied;
    }

    /**
     * Gets the instant app intent for the given URL if one exists.
     */
    public Intent getInstantAppIntentForUrl(String url) {
        return null;
    }

    /**
     * Returns a VrHandler for Page Info UI.
     */
    public VrHandler getVrHandler() {
        return mVrHandler;
    }

    /**
     * Gets the Url of the offline page being shown if any. Returns null otherwise.
     */
    @Nullable
    public String getOfflinePageUrl() {
        return mOfflinePageUrl;
    }

    /**
     * Whether the page being shown is an offline page.
     */
    public boolean isShowingOfflinePage() {
        return mOfflinePageState != OfflinePageState.NOT_OFFLINE_PAGE && !isShowingPreview();
    }

    /**
     * Initialize viewParams with Offline Page UI info, if any.
     * @param viewParams The PageInfoViewParams to set state on.
     * @param runAfterDismiss Used to set "open Online" button callback for offline page.
     */
    public void initOfflinePageUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        viewParams.openOnlineButtonShown = false;
    }

    /**
     * Return the connection message shown for an offline page, if appropriate.
     * Returns null if there's no offline page.
     */
    @Nullable
    public String getOfflinePageConnectionMessage() {
        return null;
    }

    /**
     * Returns whether or not the performance badge should be shown for |url|.
     */
    public boolean shouldShowPerformanceBadge(String url) {
        return false;
    }

    /**
     * Whether Site settings are available.
     */
    public boolean isSiteSettingsAvailable() {
        return mIsSiteSettingsAvailable;
    }

    /**
     * Show site settings for the URL passed in.
     * @param url The URL to show site settings for.
     */
    public abstract void showSiteSettings(String url);

    /**
     * Show cookie settings.
     */
    public abstract void showCookieSettings();

    /**
     * Creates Cookie Controls Bridge.
     * @param observer The CookieControlsObserver to create the bridge with.
     * @return the object that facilitates interfacing with native code.
     */
    @NonNull
    public abstract CookieControlsBridge createCookieControlsBridge(
            CookieControlsObserver observer);

    /**
     * @return Returns the browser context associated with this dialog.
     */
    @NonNull
    public abstract BrowserContextHandle getBrowserContext();

    /**
     * @return Returns the SiteSettingsClient for this page info.
     */
    @NonNull
    public abstract SiteSettingsClient getSiteSettingsClient();

    /**
     * Fetches a favicon for the current page and passes it to callback.
     * The UI will use a fallback icon if null is supplied.
     */
    public abstract void getFavicon(String url, Callback<Drawable> callback);
}
