// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.webapps;

import android.net.Uri;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.net.test.EmbeddedTestServer;

/** Computes URL of webapp test pages with the passed in Web Manifest URL. */
public class WebappTestPage {
    public static final String PAGE_TITLE = "Web app banner test page";

    private static final String WEB_APP_TEST_PAGE =
            "/chrome/test/data/banners/manifest_test_page.html";

    /** Returns the URL of a page with an installable Web App Manifest. */
    public static String getTestUrl(EmbeddedTestServer testServer) {
        return testServer.getURL(WEB_APP_TEST_PAGE);
    }

    /** Returns the URL of a page with the specified Web App Manifest URL. */
    public static String getTestUrlWithManifest(EmbeddedTestServer testServer, String manifestUrl) {
        String url = testServer.getURL(WEB_APP_TEST_PAGE);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with an installable Web App Manifest, and the specified action
     * query parameter.
     */
    public static String getTestUrlWithAction(EmbeddedTestServer testServer, String action) {
        String url = testServer.getURL(WEB_APP_TEST_PAGE);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with the specified Web App Manifest URL and action query parameter.
     */
    public static String getTestUrlWithManifestAndAction(
            EmbeddedTestServer testServer, String manifestUrl, String action) {
        String url = testServer.getURL(WEB_APP_TEST_PAGE);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /** Navigates to a page with a service worker and the specified Web App Manifest URL. */
    public static void navigateToPageWithManifest(
            EmbeddedTestServer testServer, Tab tab, String manifestUrl) throws Exception {
        TabLoadObserver observer = new TabLoadObserver(tab, PAGE_TITLE, null);
        observer.fullyLoadUrl(getTestUrlWithManifest(testServer, manifestUrl));
    }
}
