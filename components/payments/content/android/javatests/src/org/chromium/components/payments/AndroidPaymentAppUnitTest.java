// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Tests for the native Android payment app finder. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AndroidPaymentAppUnitTest {
    @Mock
    private AndroidPaymentApp.Launcher mLauncherMock;

    private String mErrorMessage;
    private String mPaymentMethodName;
    private String mPaymentDetails;
    private boolean mReadyToPayResponse;
    private boolean mReadyToPayQueryFinished;
    private boolean mInvokePaymentAppFinished;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testSuccessfulPaymentWithoutIsReadyToPayService() throws Exception {
        runTest(Activity.RESULT_OK);
        Assert.assertNull(mErrorMessage);
        Assert.assertEquals("https://company.com/pay", mPaymentMethodName);
        Assert.assertEquals("{}", mPaymentDetails);
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testCancelledPaymentWithoutIsReadyToPayService() throws Exception {
        runTest(Activity.RESULT_CANCELED);
        Assert.assertEquals("Payment app returned RESULT_CANCELED code. This is how payment apps "
                        + "can close their activity programmatically.",
                mErrorMessage);
        Assert.assertNull(mPaymentMethodName);
        Assert.assertNull(mPaymentDetails);
    }

    private void runTest(int resultCode) throws Exception {
        MockitoAnnotations.initMocks(this);

        // Reset test results.
        mErrorMessage = null;
        mPaymentMethodName = null;
        mPaymentDetails = null;
        mReadyToPayResponse = false;
        mReadyToPayQueryFinished = false;
        mInvokePaymentAppFinished = false;

        AndroidPaymentApp app = new AndroidPaymentApp(mLauncherMock, "com.company.app",
                "com.company.app.PaymentActivity",
                /*isReadyToPayService=*/null, "App Label", /*icon=*/null, /*isIncognito=*/false,
                /*appToHide=*/null, new SupportedDelegations());
        app.addMethodName("https://company.com/pay");

        Map<String, PaymentMethodData> methods = new HashMap<>();
        methods.put("https://company.com/pay", new PaymentMethodData());

        Map<String, PaymentDetailsModifier> modifiers = new HashMap<>();

        app.maybeQueryIsReadyToPayService(methods, "https://merchant.com", "https://psp.com",
                /*certificateChain=*/null, modifiers, new AndroidPaymentApp.IsReadyToPayCallback() {
                    @Override
                    public void onIsReadyToPayResponse(
                            AndroidPaymentApp app, boolean isReadyToPay) {
                        mReadyToPayQueryFinished = true;
                        mReadyToPayResponse = isReadyToPay;
                    }
                });
        CriteriaHelper.pollUiThreadNested(() -> mReadyToPayQueryFinished);
        Assert.assertTrue("Payment app should be ready to pay", mReadyToPayResponse);

        PaymentItem total = new PaymentItem();
        total.amount = new PaymentCurrencyAmount();
        total.amount.currency = "USD";
        total.amount.value = "1.00";
        total.label = "Total";
        app.invokePaymentApp("request-id", "Merchant Name", "https://merchant.com",
                "https://psp.com", /*certificateChain=*/null, methods, total,
                /*displayItems=*/new ArrayList<PaymentItem>(), modifiers, new PaymentOptions(),
                new ArrayList<PaymentShippingOption>(), new PaymentApp.InstrumentDetailsCallback() {
                    @Override
                    public void onInstrumentDetailsReady(
                            String methodName, String stringifiedDetails, PayerData payerData) {
                        mPaymentMethodName = methodName;
                        mPaymentDetails = stringifiedDetails;
                        mInvokePaymentAppFinished = true;
                    }

                    @Override
                    public void onInstrumentDetailsError(String errorMessage) {
                        mErrorMessage = errorMessage;
                        mInvokePaymentAppFinished = true;
                    }
                });
        AndroidPaymentApp.IntentResult intentResult = new AndroidPaymentApp.IntentResult();
        intentResult.resultCode = resultCode;
        intentResult.data = new Intent();
        Bundle extras = new Bundle();
        extras.putString("methodName", "https://company.com/pay");
        extras.putString("details", "{}");
        intentResult.data.putExtras(extras);
        app.onIntentCompletedForTesting(intentResult);

        CriteriaHelper.pollUiThreadNested(() -> mInvokePaymentAppFinished);
    }
}
