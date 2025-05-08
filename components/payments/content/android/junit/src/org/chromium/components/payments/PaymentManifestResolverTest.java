// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Set;

/** Tests for determining the set of URL payment methods whose manifests should be downloaded. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentManifestResolverTest {
    /**
     * Download the manifest for an app's default payment method that is supported by the merchant.
     */
    @Test
    @Feature({"Payments"})
    public void testDownloadDefaultPaymentMethodSupportedByMerchant() throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://alice.example"),
                        /* appOtherPaymentMethods= */ Set.of(),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example")));

        assertEquals(Set.of(new GURL("https://alice.example")), actualManifestsToDownload);
    }

    /**
     * Do not download the manifest for an app's default payment method that is not supported by the
     * merchant.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadDefaultPaymentMethodNotSupportedByMerchant() throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://bob.example"),
                        /* appOtherPaymentMethods= */ Set.of(),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example")));

        assertTrue(actualManifestsToDownload.isEmpty());
    }

    /**
     * Download the manifests both for an app's default payment method (to verify package name,
     * version, and signature) and an other (non-default) payment method, when the merchant supports
     * only the other (non-default) payment method.
     */
    @Test
    @Feature({"Payments"})
    public void testDownloadDefaultAndOtherPaymentMethodWhenMerchantSupportsOnlyOther()
            throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://bob.example"),
                        /* appOtherPaymentMethods= */ Set.of(
                                new GURL("https://alice.example"),
                                new GURL("https://charlie.example")),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example"),
                                new GURL("https://david.example")));

        assertEquals(
                Set.of(new GURL("https://alice.example"), new GURL("https://bob.example")),
                actualManifestsToDownload);
    }

    /**
     * Do not download the manifest for an app's other (non-default) payment method that is not
     * supported by the merchant.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadOtherPaymentMethodNotSupportedByMerchant() throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://bob.example"),
                        /* appOtherPaymentMethods= */ Set.of(
                                new GURL("https://charlie.example"),
                                new GURL("https://david.example")),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example"),
                                new GURL("https://evan.example")));

        assertTrue(actualManifestsToDownload.isEmpty());
    }

    /** Do not download the manifest for a null default payment method. */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadNullDefaultPaymentMethod() throws Exception {
        Set<GURL> merchantSupportedPaymentMethods = new HashSet<>();
        merchantSupportedPaymentMethods.add(new GURL("https://alice.example"));
        merchantSupportedPaymentMethods.add(null);

        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ null,
                        /* appOtherPaymentMethods= */ Set.of(new GURL("https://charlie.example")),
                        merchantSupportedPaymentMethods);

        assertTrue(actualManifestsToDownload.isEmpty());
    }

    /** Do not download the manifest for a default payment method that is an invalid URL. */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadInvalidDefaultPaymentMethod() throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("basic-card"),
                        /* appOtherPaymentMethods= */ Set.of(new GURL("https://charlie.example")),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example"), new GURL("basic-card")));

        assertTrue(actualManifestsToDownload.isEmpty());
    }

    /** Do not download the manifest for a null other (non-default) payment method. */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadNullOtherPaymentMethod() throws Exception {
        Set<GURL> appOtherPaymentMethods = new HashSet<>();
        appOtherPaymentMethods.add(new GURL("https://david.example"));
        appOtherPaymentMethods.add(null);
        Set<GURL> merchantSupportedPaymentMethods = new HashSet<>();
        merchantSupportedPaymentMethods.add(new GURL("https://alice.example"));
        merchantSupportedPaymentMethods.add(null);

        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://bob.example"),
                        appOtherPaymentMethods,
                        merchantSupportedPaymentMethods);

        assertTrue(actualManifestsToDownload.isEmpty());
    }

    /** Do not download the manifest for an other (non-default) payment method that is invalid. */
    @Test
    @Feature({"Payments"})
    public void testNoDownloadInvalidOtherPaymentMethod() throws Exception {
        Set<GURL> actualManifestsToDownload =
                PaymentManifestResolver.getManifestsToDownload(
                        /* appDefaultPaymentMethod= */ new GURL("https://bob.example"),
                        /* appOtherPaymentMethods= */ Set.of(
                                new GURL("basic-card"), new GURL("https://charlie.example")),
                        /* merchantSupportedPaymentMethods= */ Set.of(
                                new GURL("https://alice.example"), new GURL("basic-card")));

        assertTrue(actualManifestsToDownload.isEmpty());
    }
}
