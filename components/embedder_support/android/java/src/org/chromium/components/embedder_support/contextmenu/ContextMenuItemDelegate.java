// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.net.Uri;

import androidx.annotation.IntDef;

import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A delegate responsible for taking actions based on context menu selections. */
public interface ContextMenuItemDelegate {
    // The type of the data to save to the clipboard.
    @IntDef({ClipboardType.LINK_URL, ClipboardType.LINK_TEXT, ClipboardType.IMAGE_URL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClipboardType {
        int LINK_URL = 0;
        int LINK_TEXT = 1;
        int IMAGE_URL = 2;
    }

    /** Called when this ContextMenuItemDelegate is about to be destroyed. */
    void onDestroy();

    /**
     * @return The title of the current page associated with this delegate..
     */
    String getPageTitle();

    /**
     * @return The web contents of the current page owned by this delegate.
     */
    WebContents getWebContents();

    /**
     * @return Whether or not this context menu is being shown for an incognito content.
     */
    boolean isIncognito();

    /**
     * @return Whether or not the current application can show incognito pages.
     */
    boolean isIncognitoSupported();

    /**
     * @return Whether the embedder can get itself into multi-window mode.
     */
    boolean canEnterMultiWindowMode();

    /**
     * Called when the context menu is trying to start a download.
     *
     * @param url Url of the download item.
     * @param isLink Whether or not the download is a link (as opposed to an image/video).
     * @return Whether or not a download should actually be started.
     */
    boolean startDownload(GURL url, boolean isLink);

    /**
     * Called when the {@code text} should be saved to the clipboard.
     *
     * @param text The text to save to the clipboard.
     * @param clipboardType The type of data in {@code text}.
     */
    void onSaveToClipboard(String text, @ClipboardType int clipboardType);

    /**
     * Called when the image should be saved to the clipboard.
     *
     * @param Uri The (@link Uri) of the image to save to the clipboard.
     */
    void onSaveImageToClipboard(Uri uri);

    /**
     * @return whether an activity is available to handle an intent to call a phone number.
     */
    public boolean supportsCall();

    /**
     * Called when the {@code url} should be parsed to call a phone number.
     *
     * @param url The URL to be parsed to call a phone number.
     */
    void onCall(GURL url);

    /**
     * @return whether an activity is available to handle an intent to send an email.
     */
    public boolean supportsSendEmailMessage();

    /**
     * Called when the {@code url} should be parsed to send an email.
     *
     * @param url The URL to be parsed to send an email.
     */
    void onSendEmailMessage(GURL url);

    /**
     * @return whether an activity is available to handle an intent to send a text message.
     */
    public boolean supportsSendTextMessage();

    /**
     * Called when the {@code url} should be parsed to send a text message.
     *
     * @param url The URL to be parsed to send a text message.
     */
    void onSendTextMessage(GURL url);

    /**
     * Returns whether or not an activity is available to handle intent to add contacts.
     *
     * @return true if an activity is available to handle intent to add contacts.
     */
    public boolean supportsAddToContacts();

    /**
     * Called when the {@code url} should be parsed to add to contacts.
     *
     * @param url The URL to be parsed to add to contacts.
     */
    void onAddToContacts(GURL url);

    /**
     * @return page url.
     */
    GURL getPageUrl();

    /**
     * Called when the current embedder app is not the default to handle a View Intent.
     *
     * @param url The URL to open.
     */
    void onOpenInDefaultBrowser(GURL url);
}
