// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.url.GURL;

/** Test rule for homepage related tests that remove related shared prefs after test cases. */
public class HomepageTestRule implements TestRule {
    private final SharedPreferencesManager mManager;

    public HomepageTestRule() {
        mManager = ChromeSharedPreferences.getInstance();
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    removeHomepageSharedPrefs();
                }
            }
        };
    }

    private void removeHomepageSharedPrefs() {
        mManager.removeKey(ChromePreferenceKeys.HOMEPAGE_ENABLED);
        mManager.removeKey(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI);
        mManager.removeKey(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP);
        mManager.removeKey(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL);
    }

    // Utility functions that help setting up homepage related shared preference.

    /**
     * Set homepage disabled for this test cases.
     * <pre>
     * HOMEPAGE_ENABLED -> false
     * </pre>
     */
    public void disableHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
    }

    /**
     * Set up shared preferences to use default Homepage in the testcase.
     * <pre>
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> true;
     * HOMEPAGE_USE_CHROME_NTP -> false;
     * </pre>
     */
    public void useDefaultHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
    }

    /**
     * Set up shared preferences to use Chrome NTP as homepage. This is to select chrome NTP in the
     * home settings page, rather than setting the address of Chrome NTP as customized homepage.
     *
     * <pre>
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> false;
     * HOMEPAGE_USE_CHROME_NTP -> true;
     * </pre>
     */
    public void useChromeNtpForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);
    }

    /**
     * Set up shared preferences to use customized homepage.
     * <pre>
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> false;
     * HOMEPAGE_USE_CHROME_NTP -> false;
     * HOMEPAGE_CUSTOM_URI -> |homepage|
     * </pre>
     * @param homepage The customized homepage that will be used in this testcase.
     */
    public void useCustomizedHomepageForTest(String homepage) {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
        mManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, new GURL(homepage).serialize());
    }
}
