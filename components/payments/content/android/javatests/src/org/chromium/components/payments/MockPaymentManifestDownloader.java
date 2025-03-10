// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * An override for the downloader with static responses instead of querying servers on the network.
 * The downloader can return one of two manifest files, both of which contain information for an
 * Android payment app.
 *
 * <p>The contents of the two manifests are setup in such a way that the first app's signature is
 * "AABBCCDDEEFF001122334455" and package name is "test.payments.app".
 *
 * <p>The second app's signature is ""001122334455AABBCCDDEEFF" and package name is
 * "test.payments.other.app".
 */
public class MockPaymentManifestDownloader extends PaymentManifestDownloader {
    // The fingerprints[0].value is the SHA256 of the string "AABBCCDDEEFF001122334455".
    private static final String MANIFEST_JSON =
            """
        {
            "default_applications": ["/web-pay/manifest.json"],
            "related_applications": [{
                "platform": "play",
                "id": "test.payments.app",
                "min_version": "1",
                "fingerprints": [{
                  "type": "sha256_cert",
                  "value": "79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE:06:50:CE:E4:75:80:62:65:02:FB:F6:F9:91:AB:6E:BE:21:7E"
                }]
            }]
        }
        """;

    // The fingerprints[0].value is the SHA256 of the string "001122334455AABBCCDDEEFF".
    private static final String OTHER_MANIFEST_JSON =
            """
        {
            "default_applications": ["/web-pay/manifest.json"],
            "related_applications": [{
                "platform": "play",
                "id": "test.payments.other.app",
                "min_version": "1",
                "fingerprints": [{
                  "type": "sha256_cert",
                  "value": "01:9D:A6:93:7D:A2:1D:64:25:D8:D4:93:37:29:55:20:D9:54:16:A0:99:DD:E3:CA:31:EE:94:A4:70:AD:BD:70"
                }]
            }]
        }
        """;

    private final String mPaymentMethodName;

    /**
     * Constructs a fake downloader.
     *
     * @param paymentMethodName The payment method name matching the manifest with package name
     *     "test.payments.app", and this manifest is returned for this paymentMethodName. Otherwise
     *     the second manifest with package name "test.payments.other.app" is returned for any other
     *     paymentMethodName.
     */
    public MockPaymentManifestDownloader(String paymentMethodName) {
        mPaymentMethodName = paymentMethodName;
    }

    @Override
    public void downloadPaymentMethodManifest(
            Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
        callback.onPaymentMethodManifestDownloadSuccess(
                url,
                Origin.create(url),
                url.getSpec().equals(mPaymentMethodName) ? MANIFEST_JSON : OTHER_MANIFEST_JSON);
    }

    @Override
    public void downloadWebAppManifest(
            Origin paymentMethodManifestOrigin, GURL url, ManifestDownloadCallback callback) {
        callback.onWebAppManifestDownloadSuccess(
                url.getSpec().startsWith(mPaymentMethodName) ? MANIFEST_JSON : OTHER_MANIFEST_JSON);
    }
}
