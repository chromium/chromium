// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.HashMap;
import java.util.Map;

/**
 * An override for the downloader with static responses instead of querying servers on the network.
 * The downloader can return one of two manifest files, both of which contain information for an
 * Android payment app.
 *
 * <p>Sample usage:
 *
 * <pre>
 *    AndroidPaymentAppFinder.setDownloaderForTest(new MockPaymentManifestDownloader()
 *        .serveManifestFor(new MockPaymentApp()
 *                .setPackage("test.payments.app")
 *                .setMethod("https://payments.test/web-pay")
 *                .setSha256CertificateFingerprint("79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9"))
 *        .serveManifestFor(new MockPaymentApp()
 *                .setPackage("test.payments.app")
 *                .setMethod("https://payments.test/web-pay")
 *                .setSha256CertificateFingerprint("79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9")));
 * </pre>
 */
public class MockPaymentManifestDownloader extends PaymentManifestDownloader {
    private static final String MANIFEST_JSON_FORMAT =
            """
        {
            "default_applications": ["/web-pay/manifest.json"],
            "related_applications": [{
                "platform": "play",
                "id": "%s",
                "min_version": "1",
                "fingerprints": [{
                  "type": "sha256_cert",
                  "value": "%s"
                }]
            }]
        }
        """;

    // A mapping of the payment method name's hostname (e.g., "payments.test") to the default mock
    // payment app of this payment method.
    private final Map<String, MockPaymentApp> mApps = new HashMap<>();

    /**
     * Adds a mock payment app to the list of apps for which this downloader will return manifest
     * contents.
     *
     * @param app The mock payment app to the list of apps for which this downloader will return
     *     manifest contents. Each mock payment app's method name must have a different hostname.
     * @return A reference to this {@link MockPaymentManifestDownloader} instance.
     */
    public MockPaymentManifestDownloader serveManifestFor(MockPaymentApp app) {
        String hostname = new GURL(app.getMethod()).getHost();
        assert !mApps.containsKey(hostname)
                : "Each mock payment app's method name must have a different hostname.";
        mApps.put(hostname, app);
        return this;
    }

    // PaymentManifestDownloader:
    @Override
    public void downloadPaymentMethodManifest(
            Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
        MockPaymentApp app = mApps.get(url.getHost());
        if (app == null) {
            callback.onManifestDownloadFailure(url.getSpec() + " mock payment app not found.");
            return;
        }

        callback.onPaymentMethodManifestDownloadSuccess(
                url,
                Origin.create(url),
                String.format(
                        MANIFEST_JSON_FORMAT,
                        app.getPackage(),
                        app.getSha256CertificateFingerprint()));
    }

    // PaymentManifestDownloader:
    @Override
    public void downloadWebAppManifest(
            Origin paymentMethodManifestOrigin, GURL url, ManifestDownloadCallback callback) {
        MockPaymentApp app = mApps.get(url.getHost());
        if (app == null) {
            callback.onManifestDownloadFailure(url.getSpec() + " mock payment app not found.");
            return;
        }

        callback.onWebAppManifestDownloadSuccess(
                String.format(
                        MANIFEST_JSON_FORMAT,
                        app.getPackage(),
                        app.getSha256CertificateFingerprint()));
    }
}
