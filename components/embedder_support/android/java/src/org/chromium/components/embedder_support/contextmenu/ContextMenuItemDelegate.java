// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A delegate responsible for taking actions based on context menu selections. */
@NullMarked
public interface ContextMenuItemDelegate {
    // The type of the data to save to the clipboard.
    @IntDef({ClipboardType.LINK_URL, ClipboardType.LINK_TEXT, ClipboardType.IMAGE_URL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ClipboardType {
        int LINK_URL = 0;
        int LINK_TEXT = 1;
        int IMAGE_URL = 2;
    }

    /** Called when this ContextMenuItemDelegate is about to be destroyed. */
    void onDestroy();

    /** Returns the title of the current page associated with this delegate.. */
    String getPageTitle();

    /** Returns the web contents of the current page owned by this delegate. */
    WebContents getWebContents();

    /** Returns whether this context menu is being shown for an incognito content. */
    default boolean isIncognito() {
        return false;
    }

    /** Returns whether the current application can show incognito pages. */
    default boolean isIncognitoSupported() {
        return false;
    }

    /** Returns whether the current profile enables printing. */
    default boolean isPrintSupported() {
        return false;
    }

    /** Returns whether the "Open in other window" context menu item should be shown. */
    default boolean isOpenInOtherWindowSupported() {
        return false;
    }

    /**
     * Called when the context menu is trying to start a download.
     *
     * @param url Url of the download item.
     * @param isLink Whether or not the download is a link (as opposed to an image/video).
     * @return Whether or not a download should actually be started.
     */
    default boolean startDownload(GURL url, boolean isLink) {
        return false;
    }

    /**
     * Called when the context menu is trying to start a download of the current page.
     *
     * @param context The context to use for the download.
     */
    default void startDownloadPage(Context context) {}

    /** Initiates the printing process of the current page. */
    default void startPrint() {}

    /**
     * Called when the {@code text} should be saved to the clipboard.
     *
     * @param text The text to save to the clipboard.
     * @param clipboardType The type of data in {@code text}.
     */
    default void onSaveToClipboard(String text, @ClipboardType int clipboardType) {}

    /**
     * Called when the image should be saved to the clipboard.
     *
     * @param Uri The (@link Uri) of the image to save to the clipboard.
     */
    default void onSaveImageToClipboard(Uri uri) {}

    /** Returns whether an activity is available to handle an intent to call a phone number. */
    default boolean supportsCall() {
        return false;
    }

    /**
     * Called when the {@code url} should be parsed to call a phone number.
     *
     * @param url The URL to be parsed to call a phone number.
     */
    default void onCall(GURL url) {}

    /** Returns whether an activity is available to handle an intent to send an email. */
    default boolean supportsSendEmailMessage() {
        return false;
    }

    /**
     * Called when the {@code url} should be parsed to send an email.
     *
     * @param url The URL to be parsed to send an email.
     */
    default void onSendEmailMessage(GURL url) {}

    /** Returns whether an activity is available to handle an intent to send a text message. */
    default boolean supportsSendTextMessage() {
        return false;
    }

    /**
     * Called when the {@code url} should be parsed to send a text message.
     *
     * @param url The URL to be parsed to send a text message.
     */
    default void onSendTextMessage(GURL url) {}

    /**
     * Returns whether an activity is available to handle intent to add contacts.
     *
     * @return true if an activity is available to handle intent to add contacts.
     */
    default boolean supportsAddToContacts() {
        return false;
    }

    /**
     * Called when the {@code url} should be parsed to add to contacts.
     *
     * @param url The URL to be parsed to add to contacts.
     */
    default void onAddToContacts(GURL url) {}

    /**
     * @return Whether opening an image in a new tab is supported.
     */
    default boolean supportsOpenImageInNewTab() {
        return false;
    }

    /**
     * @return Whether opening an ephemeral preview tab is supported.
     */
    default boolean supportsOpenInEphemeralTab() {
        return false;
    }

    /**
     * @return Whether saving/downloading an image is supported.
     */
    default boolean supportsSaveImage() {
        return false;
    }

    /**
     * @return Whether saving/downloading a link is supported.
     */
    default boolean supportsSaveLinkAs() {
        return false;
    }

    /**
     * @return Whether searching by image / Google Lens is supported.
     */
    default boolean supportsSearchByImage() {
        return false;
    }

    /**
     * @return Whether inspecting elements is supported.
     */
    default boolean supportsInspectElement() {
        return false;
    }

    /**
     * Called when the {@code url} is of an image and should be opened in a new page.
     *
     * @param url The image URL to open.
     * @param referrer The referrer to use when opening the URL.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void onOpenImageInNewTab(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * Called when the {@code url} should be opened in an ephemeral page.
     *
     * @param url The URL to open.
     * @param title The title text to show on top control.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void onOpenInEphemeralTab(
            GURL url,
            String title,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * @return Whether opening a link in a new tab is supported.
     */
    default boolean supportsOpenInNewTab() {
        return false;
    }

    /**
     * @return Whether opening a link in a new tab in group is supported.
     */
    default boolean supportsOpenInNewTabInGroup() {
        return false;
    }

    /**
     * @return Whether opening a link in a new incognito tab is supported.
     */
    default boolean supportsOpenInNewIncognitoTab() {
        return false;
    }

    /**
     * @return Whether opening a link in a new window is supported.
     */
    default boolean supportsOpenInNewWindow() {
        return false;
    }

    /**
     * @return Whether opening a link in an incognito window is supported.
     */
    default boolean supportsOpenInIncognitoWindow() {
        return false;
    }

    /**
     * Called when the {@code url} should be opened in a new page with the same incognito state as
     * the current page.
     *
     * @param url The URL to open.
     * @param referrer The referrer to use when opening the URL.
     * @param navigateToTab Whether or not to navigate to the new page.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void onOpenInNewTab(
            GURL url,
            @Nullable Referrer referrer,
            boolean navigateToTab,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * Called when {@code url} should be opened in a new page in the same group as the current page.
     *
     * @param url The URL to open.
     * @param referrer The referrer to use when opening the URL.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void onOpenInNewTabInGroup(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * Called when the {@code url} should be opened in a new incognito page.
     *
     * @param url The URL to open.
     */
    default void onOpenInNewIncognitoTab(GURL url) {}

    /**
     * Opens a URL in a new or existing window.
     *
     * @param url The URL to open.
     * @param referrer The referrer to use when opening the URL.
     * @param isIncognito Whether the other window should be incognito.
     * @param preferNew Whether the URL should be opened in a new window.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void openInOtherWindow(
            GURL url,
            @Nullable Referrer referrer,
            boolean isIncognito,
            boolean preferNew,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * Opens a URL in an incognito window.
     *
     * @param url The URL to open.
     */
    default void openInIncognitoWindow(GURL url) {}

    /**
     * @return Whether adding a link to the reading list is supported.
     */
    default boolean supportsReadLater() {
        return false;
    }

    /**
     * Called when Read Later was selected from the context menu.
     *
     * @param url The URL to be saved to the reading list.
     * @param title The title text to be shown for this item in the reading list.
     */
    default void onReadLater(GURL url, String title) {}

    /**
     * Called when the {@code url} is of an image and should be opened in the same page.
     *
     * @param url The image URL to open.
     * @param referrer The referrer to use when opening the URL.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    default void onOpenImageUrl(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {}

    /**
     * Called when a link should be opened in the main Chrome browser.
     *
     * @param linkUrl URL that should be opened.
     * @param pageUrl URL of the current page.
     */
    default void onOpenInChrome(GURL linkUrl, GURL pageUrl) {}

    /**
     * Called when the {@code url} should be opened in a new Chrome page from CCT.
     *
     * @param linkUrl The URL to open.
     * @param isIncognito true if the {@code url} should be opened in a new incognito page.
     */
    default void onOpenInNewChromeTabFromCct(GURL linkUrl, boolean isIncognito) {}

    /** Returns the page url. */
    GURL getPageUrl();

    /**
     * Called when the current embedder app is not the default to handle a View Intent.
     *
     * @param url The URL to open.
     */
    void onOpenInDefaultBrowser(GURL url);

    /** Called when the current tab should be reloaded. */
    default void onReloadCurrentTab() {}
}
