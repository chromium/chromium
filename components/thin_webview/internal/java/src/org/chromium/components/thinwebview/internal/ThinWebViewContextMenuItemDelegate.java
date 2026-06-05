// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import android.content.Context;
import android.content.Intent;
import android.net.MailTo;
import android.net.Uri;
import android.provider.ContactsContract;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Handles the context menu item functionality in WebView. */
@NullMarked
public class ThinWebViewContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final WebContents mWebContents;

    /** Builds a {@link ThinWebViewContextMenuItemDelegate} instance. */
    public ThinWebViewContextMenuItemDelegate(WebContents webContents) {
        mWebContents = webContents;
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

    private void safeStartActivity(Intent intent) {
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window != null) {
            Context context = window.getActivity().get();
            if (context != null) {
                IntentUtils.safeStartActivity(context, intent);
            }
        }
    }
}
