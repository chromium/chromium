// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import android.content.Context;
import android.content.Intent;
import android.net.MailTo;
import android.net.Uri;
import android.provider.ContactsContract;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.thinwebview.ThinWebViewPrintingController;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.function.BiConsumer;

/** Handles the context menu item functionality in WebView. */
@NullMarked
public class ThinWebViewContextMenuItemDelegate implements ContextMenuItemDelegate {
    public interface LinkOpener {
        void openInNewTab(
                GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams);

        void openInNewTabInGroup(
                GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams);

        void openInNewIncognitoTab(GURL url);

        void openInNewWindow(
                GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams);

        void openInIncognitoWindow(GURL url);

        boolean isIncognitoSupported();
    }

    private final WebContents mWebContents;
    private final @Nullable String mIntentTargetClassName;
    private final @Nullable BiConsumer<GURL, String> mEphemeralTabOpener;
    private final @Nullable ThinWebViewPrintingController mPrintingController;
    private final @Nullable LinkOpener mLinkOpener;
    private final @Nullable BiConsumer<GURL, String> mReadLaterOpener;

    /** Builds a {@link ThinWebViewContextMenuItemDelegate} instance. */
    public ThinWebViewContextMenuItemDelegate(WebContents webContents) {
        this(
                webContents,
                /* intentTargetClassName= */ null,
                /* ephemeralTabOpener= */ null,
                /* linkOpener= */ null,
                /* readLaterOpener= */ null);
    }

    /**
     * Builds a {@link ThinWebViewContextMenuItemDelegate} instance.
     *
     * @param webContents The WebContents for the ThinWebView.
     * @param intentTargetClassName The fully qualified class name used as the explicit component
     *     for Intents fired by this delegate. Required for environments where the window's activity
     *     context might be null.
     * @param ephemeralTabOpener A callback to open a URL in an ephemeral tab, if supported.
     */
    public ThinWebViewContextMenuItemDelegate(
            WebContents webContents,
            @Nullable String intentTargetClassName,
            @Nullable BiConsumer<GURL, String> ephemeralTabOpener) {
        this(webContents, intentTargetClassName, ephemeralTabOpener, null, null);
    }

    /**
     * Builds a {@link ThinWebViewContextMenuItemDelegate} instance.
     *
     * @param webContents The WebContents for the ThinWebView.
     * @param intentTargetClassName The fully qualified class name used as the explicit component
     *     for Intents fired by this delegate. Required for environments where the window's activity
     *     context might be null.
     * @param ephemeralTabOpener A callback to open a URL in an ephemeral tab, if supported.
     * @param linkOpener A delegate to handle link opening actions.
     * @param readLaterOpener A callback to add a URL to the reading list, if supported.
     */
    public ThinWebViewContextMenuItemDelegate(
            WebContents webContents,
            @Nullable String intentTargetClassName,
            @Nullable BiConsumer<GURL, String> ephemeralTabOpener,
            @Nullable LinkOpener linkOpener,
            @Nullable BiConsumer<GURL, String> readLaterOpener) {
        mWebContents = webContents;
        mIntentTargetClassName = intentTargetClassName;
        mEphemeralTabOpener = ephemeralTabOpener;
        mPrintingController = ServiceLoaderUtil.maybeCreate(ThinWebViewPrintingController.class);
        mLinkOpener = linkOpener;
        mReadLaterOpener = readLaterOpener;
    }

    @Override
    public void onDestroy() {}

    @Override
    public String getPageTitle() {
        return mWebContents.getTitle();
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public void onSaveToClipboard(String text, int clipboardType) {
        Clipboard.getInstance().setText(text);
    }

    @Override
    public void onSaveImageToClipboard(Uri uri) {
        Clipboard.getInstance().setImageUri(uri);
    }

    @Override
    public boolean supportsCall() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("tel:"));
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        return window != null && window.canResolveActivity(intent);
    }

    @Override
    public void onCall(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url.getSpec()));
        safeStartActivity(intent);
    }

    @Override
    public boolean supportsSendEmailMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("mailto:test@example.com"));
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        return window != null && window.canResolveActivity(intent);
    }

    @Override
    public void onSendEmailMessage(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url.getSpec()));
        safeStartActivity(intent);
    }

    @Override
    public boolean supportsSendTextMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("sms:"));
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        return window != null && window.canResolveActivity(intent);
    }

    @Override
    public void onSendTextMessage(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("sms:" + UrlUtilities.getTelNumber(url)));
        safeStartActivity(intent);
    }

    @Override
    public boolean supportsAddToContacts() {
        Intent intent = new Intent(Intent.ACTION_INSERT);
        intent.setType(ContactsContract.Contacts.CONTENT_TYPE);
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        return window != null && window.canResolveActivity(intent);
    }

    @Override
    public void onAddToContacts(GURL url) {
        Intent intent = new Intent(Intent.ACTION_INSERT);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setType(ContactsContract.Contacts.CONTENT_TYPE);
        if (MailTo.isMailTo(url.getSpec())) {
            intent.putExtra(
                    ContactsContract.Intents.Insert.EMAIL,
                    MailTo.parse(url.getSpec()).getTo().split(",")[0]);
        } else if (UrlUtilities.isTelScheme(url)) {
            intent.putExtra(ContactsContract.Intents.Insert.PHONE, UrlUtilities.getTelNumber(url));
        }
        safeStartActivity(intent);
    }

    @Override
    public void onOpenInDefaultBrowser(GURL url) {
        // Intentionally empty.
    }

    @Override
    public GURL getPageUrl() {
        return mWebContents.getVisibleUrl();
    }

    @Override
    public void onReloadCurrentTab() {
        mWebContents.getNavigationController().reload(/* checkForRepost= */ true);
    }

    @Override
    public boolean startDownload(GURL url, boolean isLink) {
        return true;
    }

    @Override
    public void onOpenImageInNewTab(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        // Note: additionalNavigationParams cannot be passed across external Intent boundaries.
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url.getSpec()));
        if (referrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrer.getUrl()));
        }
        safeStartActivity(intent);
    }

    @Override
    public void onOpenInEphemeralTab(
            GURL url,
            String title,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        if (!supportsOpenInEphemeralTab()) return;

        // Note: additionalNavigationParams cannot be passed across external Intent boundaries or
        // through mEphemeralTabOpener.
        if (mEphemeralTabOpener != null) {
            mEphemeralTabOpener.accept(url, title);
        } else {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.setData(Uri.parse(url.getSpec()));
            intent.putExtra(CustomTabsIntent.EXTRA_ENABLE_EPHEMERAL_BROWSING, true);
            safeStartActivity(intent);
        }
    }

    @Override
    public boolean isPrintSupported() {
        return mPrintingController != null && mPrintingController.isPrintSupported(mWebContents);
    }

    @Override
    public void startPrint() {
        if (mPrintingController != null) {
            mPrintingController.startPrint(mWebContents);
        }
    }

    private void safeStartActivity(Intent intent) {
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window != null) {
            Context context = window.getActivity().get();
            if (context == null) {
                context = window.getContext().get();
            }
            if (context != null) {
                if (mIntentTargetClassName != null) {
                    intent.setClassName(context.getPackageName(), mIntentTargetClassName);
                }
                IntentUtils.safeStartActivity(context, intent);
            }
        }
    }

    @Override
    public boolean supportsOpenImageInNewTab() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsOpenInNewTab() {
        return mIntentTargetClassName != null && mLinkOpener != null;
    }

    @Override
    public boolean supportsOpenInNewTabInGroup() {
        return mIntentTargetClassName != null && mLinkOpener != null;
    }

    @Override
    public boolean supportsOpenInNewIncognitoTab() {
        return mIntentTargetClassName != null && mLinkOpener != null;
    }

    @Override
    public boolean supportsOpenInNewWindow() {
        return mIntentTargetClassName != null && mLinkOpener != null;
    }

    @Override
    public boolean supportsOpenInIncognitoWindow() {
        return mIntentTargetClassName != null && mLinkOpener != null;
    }

    @Override
    public boolean isIncognitoSupported() {
        return mLinkOpener != null && mLinkOpener.isIncognitoSupported();
    }

    @Override
    public void onOpenInNewTab(
            GURL url,
            @Nullable Referrer referrer,
            boolean navigateToTab,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        if (mLinkOpener != null) {
            mLinkOpener.openInNewTab(url, additionalNavigationParams);
        }
    }

    @Override
    public void onOpenInNewTabInGroup(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        if (mLinkOpener != null) {
            mLinkOpener.openInNewTabInGroup(url, additionalNavigationParams);
        }
    }

    @Override
    public void onOpenInNewIncognitoTab(GURL url) {
        if (mLinkOpener != null) {
            mLinkOpener.openInNewIncognitoTab(url);
        }
    }

    @Override
    public void openInOtherWindow(
            GURL url,
            @Nullable Referrer referrer,
            boolean isIncognito,
            boolean preferNew,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        if (mLinkOpener != null) {
            if (isIncognito) {
                mLinkOpener.openInIncognitoWindow(url);
            } else {
                mLinkOpener.openInNewWindow(url, additionalNavigationParams);
            }
        }
    }

    @Override
    public void openInIncognitoWindow(GURL url) {
        if (mLinkOpener != null) {
            mLinkOpener.openInIncognitoWindow(url);
        }
    }

    @Override
    public boolean supportsOpenInEphemeralTab() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsSaveImage() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsSaveLinkAs() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsSearchByImage() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsInspectElement() {
        return mIntentTargetClassName != null;
    }

    @Override
    public boolean supportsReadLater() {
        return mIntentTargetClassName != null && mReadLaterOpener != null;
    }

    @Override
    public void onReadLater(GURL url, String title) {
        if (mReadLaterOpener != null) {
            mReadLaterOpener.accept(url, title);
        }
    }

    public @Nullable String getIntentTargetClassNameForTesting() {
        return mIntentTargetClassName;
    }
}
