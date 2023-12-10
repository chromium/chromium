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

    private static final String SERVICE_WORKER_PAGE_PATH =
            "/chrome/test/data/banners/manifest_test_page.html";

    private static final String NO_SERVICE_WORKER_PAGE_PATH =
            "/chrome/test/data/banners/manifest_no_service_worker.html";

    /** Returns the URL of a page with a service worker and an installable Web App Manifest. */
    public static String getServiceWorkerUrl(EmbeddedTestServer testServer) {
        String url = testServer.getURL(SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        return builder.build().toString();
    }

    /** Returns the URL of a page with no service worker and a Web App Manifest. */
    public static String getNonServiceWorkerUrl(EmbeddedTestServer testServer) {
        String url = testServer.getURL(NO_SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        return builder.build().toString();
    }

    /** Returns the URL of a page with no service worker and the specified Web App Manifest URL. */
    public static String getNonServiceWorkerUrlWithManifest(
            EmbeddedTestServer testServer, String manifestUrl) {
        String url = testServer.getURL(NO_SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with no service worker, an installable Web App Manifest, and the
     * specified action query parameter.
     */
    public static String getNonServiceWorkerUrlWithAction(
            EmbeddedTestServer testServer, String action) {
        String url = testServer.getURL(NO_SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with no service worker and the specified Web App Manifest URL and
     * action query parameter.
     */
    public static String getNonServiceWorkerUrlWithManifestAndAction(
            EmbeddedTestServer testServer, String manifestUrl, String action) {
        String url = testServer.getURL(NO_SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /** Returns the URL of a page with a service worker and the specified Web App Manifest URL. */
    public static String getServiceWorkerUrlWithManifest(
            EmbeddedTestServer testServer, String manifestUrl) {
        String url = testServer.getURL(SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with a service worker and the specified Web App Manifest URL and
     * action query parameter.
     */
    public static String getServiceWorkerUrlWithManifestAndAction(
            EmbeddedTestServer testServer, String manifestUrl, String action) {
        String url = testServer.getURL(SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("manifest", manifestUrl);
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /**
     * Returns the URL of a page with a service worker, an installable Web App Manifest, and the
     * specified action query parameter.
     */
    public static String getServiceWorkerUrlWithAction(
            EmbeddedTestServer testServer, String action) {
        String url = testServer.getURL(SERVICE_WORKER_PAGE_PATH);
        Uri.Builder builder = Uri.parse(url).buildUpon();
        builder.appendQueryParameter("action", action);
        return builder.build().toString();
    }

    /** Navigates to a page with a service worker and the specified Web App Manifest URL. */
    public static void navigateToServiceWorkerPageWithManifest(
            EmbeddedTestServer testServer, Tab tab, String manifestUrl) throws Exception {
        TabLoadObserver observer = new TabLoadObserver(tab, PAGE_TITLE, null);
        observer.fullyLoadUrl(getServiceWorkerUrlWithManifest(testServer, manifestUrl));
    }
}
