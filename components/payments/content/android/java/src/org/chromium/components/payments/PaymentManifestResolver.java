// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Set;

/**
 * Helper class for determining the set of URL payment methods whose manifests should be downloaded.
 *
 * <p>Websites specify the methods (URLs) they want to use in their Payment Request call.
 *
 * <p>Installed apps can declare both a 'default' method (URL) for their app, which acts a source of
 * truth for its identity, as well as support for methods (URLs) from another origin.
 *
 * <p>For a given request and given app, there are then three possibilities - either the request is
 * for the app itself directly (the default payment method), it is for a payment method that the app
 * claims to support, or it has no overlap at all.
 *
 * <h2>Default payment methods</h2>
 *
 * <p>Use case: the merchant supports the app's default payment method.
 *
 * <p>Download:
 *
 * <ul>
 *   <li>The payment method and web app manifests for the app's default payment method.
 * </ul>
 *
 * <p>Verify:
 *
 * <ul>
 *   <li>App identity (package name, version, signature).
 * </ul>
 *
 * <p>Example:
 *
 * <pre>
 * Merchant Website                             Alice Server
 * Supported method: https://alice.example ---&gt; https://alice.example/payment_manifest.json
 *                                                  "default_applications"
 *                                                             |
 *                                                             v
 *                                              https://alice.example/web_manifest.json
 *                                                  "related_applications"
 *                                                   |         |        |
 *                                                   v         v        v
 * Alice App &lt;----------Verify--------------- package name, version, signature
 * Default method: https://alice.example
 * </pre>
 *
 * <h2>Other payment methods</h2>
 *
 * <p>Use case: the merchant supports a payment method that is one of this app's other (non-default)
 * payment methods.
 *
 * <p>Download:
 *
 * <ul>
 *   <li>The payment method and web app manifests for the app's default payment method.
 *   <li>The payment method manifests for the payment methods that both the merchant and the app
 *       support.
 * </ul>
 *
 * <p>Verify:
 *
 * <ul>
 *   <li>App identity (package name, version, signature).
 *   <li>The {@code "supported_origins"} field is either {@code "*"} or a list of origins that
 *       includes the origin of the app's default payment method.
 * </ul>
 *
 * <p>Example:
 *
 * <pre>
 * Merchant Website                             Alice Server
 * Supported method: https://alice.example ---&gt; https://alice.example/method_manifest.json
 *                                              "supported_origins"
 *                                                   |
 *                                                   v
 *                                              Bob Server
 *                                              https://bob.example/web_manifest.json
 *                                                  "related_applications"
 *                                                   |         |        |
 *                                                   v         v        v
 * Bob App &lt;------------Verify--------------- package name, version, signature
 * Default method: https://bob.example
 * Other method: https://alice.example
 * </pre>
 *
 * <p>Learn more:
 *
 * <ul>
 *   <li><a href="https://web.dev/articles/android-payment-apps-developers-guide">Android payment
 *       apps</a>.
 *   <li><a href="https://web.dev/articles/setting-up-a-payment-method">Setting up a payment
 *       method</a>.
 *   <li><a href="https://w3c.github.io/payment-method-id/">Payment method identifier</a>.
 *   <li><a href="https://w3c.github.io/payment-method-manifest/">Payment method manifest</a>.
 * </ul>
 */
@NullMarked
/* package */ class PaymentManifestResolver {
    /**
     * Determines the set of the URL payment methods whose manifests should be downloaded for
     * verification of package name, version, signature, and "supported_origins" fields.
     *
     * @param appDefaultPaymentMethod The default payment method for the Android payment app. Leads
     *     to the web app manifest with {@code "related_applications"}, which contains this app's
     *     package name, version, and signature.
     * @param appOtherPaymentMethods Other (non-default) payment methods of this Android payment
     *     app. Leads to the payment method manifest with {@code "supported_origins"}, which can be
     *     either {@code "*"} or a list of origins.
     * @param merchantSupportedPaymentMethods The set of URI payment methods that the merchant has
     *     declared in the {@code "supportedMethods"} fields in the Payment Request JavaScript API
     *     call.
     * @return The set of URI payment methods whose manifests should be downloaded for verification.
     */
    /* package */ static Set<GURL> getManifestsToDownload(
            @Nullable GURL appDefaultPaymentMethod,
            Set<GURL> appOtherPaymentMethods,
            Set<GURL> merchantSupportedPaymentMethods) {
        // Find the intersection of all payment methods supported by the app, and the payment
        // methods requested by the website.
        Set<GURL> manifestsToDownload = new HashSet<>(appOtherPaymentMethods);
        manifestsToDownload.add(appDefaultPaymentMethod);
        manifestsToDownload.retainAll(merchantSupportedPaymentMethods);
        manifestsToDownload.removeIf(url -> !UrlUtil.isURLValid(url));

        if (manifestsToDownload.isEmpty()) {
            return manifestsToDownload;
        }

        // If the merchant requested an 'other' payment method and not the default payment method,
        // then the default method will not be included in the intersection, but the default method
        // still has to be downloaded to verify the app.
        if (UrlUtil.isURLValid(appDefaultPaymentMethod)) {
            manifestsToDownload.add(appDefaultPaymentMethod);
        }

        return manifestsToDownload;
    }

    // Do not instantiate.
    private PaymentManifestResolver() {}
}
