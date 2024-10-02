// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.content.Intent;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;

/** An interface for handling interactions for Omnibox Action Chips. */
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
    boolean startActivity(@NonNull Intent intent);

    /** Create a new incognito tab. */
    void openIncognitoTab();

    /** Open Password Manager. */
    void openPasswordManager();

    /** Open specific settings page. */
    void openSettingsPage(@SettingsFragment int fragment);

    /** Handles opening the CBD or the quick deleted dialog. */
    void handleClearBrowsingData();
}
