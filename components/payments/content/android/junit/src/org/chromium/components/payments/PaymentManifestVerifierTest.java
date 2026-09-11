// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.payments.PaymentManifestParser.PaymentMethodManifest;
import org.chromium.components.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Collections;
import java.util.Set;

/** Tests for PaymentManifestVerifier. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentManifestVerifierTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebPaymentsWebDataService mCache;
    @Mock private PaymentManifestDownloader mDownloader;
    @Mock private PaymentManifestParser mParser;
    @Mock private PackageManagerDelegate mPackageManagerDelegate;
    @Mock private ManifestVerifyCallback mCallback;

    private static final String PAYMENT_METHOD = "https://example.com/pay_v1";
    private static final String APP_PACKAGE_NAME = "com.example.pay";

    private Origin mMerchantOrigin;
    private GURL mMethodName;
    private PaymentManifestVerifier mVerifier;

    @Before
    public void setUp() {
        mMerchantOrigin = Origin.create(new GURL("https://merchant.example"));
        mMethodName = new GURL(PAYMENT_METHOD);

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = APP_PACKAGE_NAME;
        resolveInfo.activityInfo.name = "com.example.pay.PaymentActivity";

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = APP_PACKAGE_NAME;
        packageInfo.signatures = new Signature[] {new Signature("0123456789abcdef")};

        when(mPackageManagerDelegate.getPackageInfoWithSignatures(APP_PACKAGE_NAME))
                .thenReturn(packageInfo);

        mVerifier =
                new PaymentManifestVerifier(
                        mMerchantOrigin,
                        mMethodName,
                        Set.of(resolveInfo),
                        /* supportedOrigins= */ Collections.emptySet(),
                        mCache,
                        mDownloader,
                        mParser,
                        mPackageManagerDelegate,
                        mCallback);
    }

    /**
     * Verifies that when checking the web app manifest cache, the verifier queries the cache scoped
     * to the specific payment method name.
     */
    @Test
    @Feature({"Payments"})
    public void testVerifyFromCache_MethodIsolation() {
        when(mCache.getPaymentMethodManifest(eq(PAYMENT_METHOD), any())).thenReturn(true);
        when(mCache.getPaymentWebAppManifest(eq(PAYMENT_METHOD), eq(APP_PACKAGE_NAME), any()))
                .thenReturn(true);

        // Start verification. This queries the payment method manifest from the cache.
        mVerifier.verify();
        verify(mCache).getPaymentMethodManifest(eq(PAYMENT_METHOD), eq(mVerifier));

        // When the payment method manifest returns the app, the verifier must query the web app
        // manifest cache using the composite key (PAYMENT_METHOD, APP_PACKAGE_NAME).
        mVerifier.onPaymentMethodManifestFetched(new String[] {APP_PACKAGE_NAME});
        verify(mCache)
                .getPaymentWebAppManifest(eq(PAYMENT_METHOD), eq(APP_PACKAGE_NAME), eq(mVerifier));
    }

    /**
     * Verifies that when downloading and parsing manifests from the web, the web app manifest is
     * stored in the cache scoped to the specific payment method name.
     */
    @Test
    @Feature({"Payments"})
    public void testDownloadAndCache_MethodIsolation() {
        GURL webAppManifestUrl = new GURL("https://example.com/app.json");
        when(mCache.getPaymentMethodManifest(eq(PAYMENT_METHOD), any())).thenReturn(true);
        when(mParser.parsePaymentMethodManifest(eq(mMethodName), any()))
                .thenReturn(new PaymentMethodManifest(new GURL[] {webAppManifestUrl}, new GURL[0]));
        when(mParser.parseWebAppManifest(any()))
                .thenReturn(
                        new WebAppManifestSection[] {
                            new WebAppManifestSection(
                                    APP_PACKAGE_NAME,
                                    /* minVersion= */ 1,
                                    /* numberOfFingerprints= */ 0)
                        });

        // Start verification, simulating a cache miss for the payment method manifest.
        mVerifier.verify();
        mVerifier.onPaymentMethodManifestFetched(new String[0]);

        // Verifier falls back to downloading the payment method manifest from the web.
        verify(mDownloader)
                .downloadPaymentMethodManifest(eq(mMerchantOrigin), eq(mMethodName), eq(mVerifier));

        // Fake successful download of the payment method manifest, which triggers the download of
        // the default web app manifest.
        mVerifier.onPaymentMethodManifestDownloadSuccess(
                mMethodName, mMerchantOrigin, "placeholder_content");
        verify(mDownloader)
                .downloadWebAppManifest(eq(mMerchantOrigin), eq(webAppManifestUrl), eq(mVerifier));

        // Fake successful download of the web app manifest. The parsed manifest sections must now
        // be stored in the cache scoped to PAYMENT_METHOD.
        mVerifier.onWebAppManifestDownloadSuccess("placeholder_section_content");
        verify(mCache)
                .addPaymentMethodManifest(eq(PAYMENT_METHOD), eq(new String[] {APP_PACKAGE_NAME}));
        verify(mCache).addPaymentWebAppManifest(eq(PAYMENT_METHOD), any());
    }
}
