// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.url.GURL;

/** An interface for handling interactions for Omnibox Action Chips. */
@NullMarked
public interface OmniboxActionDelegate {
    /** Returns whether the user is currently browsing incognito. */
    boolean isIncognito();

    /**
     * Load the supplied URL in the current tab (if possible), or a new tab (otherwise).
     *
     * @param url the page URL to load
     */
    void loadPageInCurrentTab(String url);

    /**
     * Start the activity referenced by the supplied {@link android.content.Intent}. Decorates the
     * intent with trusted intent extras when the intent references the browser.
     *
     * @param intent the intent describing the activity to be started
     * @return whether operation was successful
     */
    boolean startActivity(Intent intent);

    /** Create a new incognito tab. */
    void openIncognitoTab();

    /** Open Password Manager. */
    void openPasswordManager();

    /** Open specific settings page. */
    void openSettingsPage(@SettingsFragment int fragment);

    /** Handles opening the CBD or the quick deleted dialog. */
    void handleClearBrowsingData();

    /**
     * Switch to an existing tab that is identified by tabId.
     *
     * @param tabid identifier for the {@link Tab}.
     * @param url the page url of the {@link Tab}.
     * @return whether the switch was successful.
     */
    boolean switchToTab(int tabId, GURL url);
}
