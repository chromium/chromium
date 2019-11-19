// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.junit.Assert.assertFalse;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.os.Build;

import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Utilities for testing the NTP.
 */
public class NewTabPageTestUtils {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    /**
     * Waits for the NTP owned by the passed in tab to be fully loaded.
     *
     * @param tab The tab to be monitored for NTP loading.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static void waitForNtpLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria("NTP never fully loaded") {
            @Override
            public boolean isSatisfied() {
                if (!tab.isIncognito()) {
                    // TODO(tedchoc): Make MostVisitedPage also have a isLoaded() concept.
                    if (tab.getNativePage() instanceof NewTabPage) {
                        return ((NewTabPage) tab.getNativePage()).isLoadedForTests();
                    } else {
                        return false;
                    }
                } else {
                    if (!(tab.getNativePage() instanceof IncognitoNewTabPage)) {
                        return false;
                    }
                    return ((IncognitoNewTabPage) tab.getNativePage()).isLoadedForTests();
                }
            }
        });
    }

    public static List<SiteSuggestion> createFakeSiteSuggestions(EmbeddedTestServer testServer) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(new SiteSuggestion("0 TOP_SITES", testServer.getURL(TEST_PAGE) + "#0",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("1 WHITELIST", testServer.getURL(TEST_PAGE) + "#1",
                "/test.png", TileTitleSource.UNKNOWN, TileSource.WHITELIST,
                TileSectionType.PERSONALIZED, new Date()));
        siteSuggestions.add(new SiteSuggestion("2 TOP_SITES", testServer.getURL(TEST_PAGE) + "#2",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("3 TOP_SITES", testServer.getURL(TEST_PAGE) + "#3",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("4 TOP_SITES", testServer.getURL(TEST_PAGE) + "#4",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("5 TOP_SITES", testServer.getURL(TEST_PAGE) + "#5",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("6 TOP_SITES", testServer.getURL(TEST_PAGE) + "#6",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("7 TOP_SITES", testServer.getURL(TEST_PAGE) + "#7",
                "", TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        return siteSuggestions;
    }

    /** Initializes {@link AccountManagerFacade} and add one dummy . */
    public static void setUpTestAccount() {
        FakeAccountManagerDelegate fakeAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(fakeAccountManager);
        Account account = AccountManagerFacade.createAccountFromName("test@gmail.com");
        fakeAccountManager.addAccountHolderExplicitly(new AccountHolder.Builder(account).build());
        assertFalse(AccountManagerFacade.get().isUpdatePending().get());
        assertFalse(SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, false));
    }
}
