// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
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
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AndroidPaymentApp.Launcher mLauncherMock;

    private String mErrorMessage;
    private String mPaymentMethodName;
    private String mPaymentDetails;
    private boolean mReadyToPayResponse;
    private boolean mReadyToPayQueryFinished;
    private boolean mInvokePaymentAppFinished;
    private Map<String, PaymentMethodData> mMethods;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        // Reset test results.
        mErrorMessage = null;
        mPaymentMethodName = null;
        mPaymentDetails = null;
        mReadyToPayResponse = false;
        mReadyToPayQueryFinished = false;
        mInvokePaymentAppFinished = false;
        mMethods = new HashMap<>();
        mMethods.put("https://company.com/pay", new PaymentMethodData());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testNoReadyToPayDebugInfo() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        Mockito.verify(mLauncherMock, Mockito.never()).showReadyToPayDebugInfo(Mockito.any());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testShowReadyToPayDebugInfo() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ true);
        queryReadyToPay(app);
        Mockito.verify(mLauncherMock, Mockito.times(1))
                .showReadyToPayDebugInfo(
                        Mockito.eq(
                                "IS_READY_TO_PAY sent to com.company.app.IsReadyToPayService in"
                                        + " com.company.app with {\"topLevelOrigin\":"
                                        + " \"https://merchant.com\", \"paymentRequestOrigin\":"
                                        + " \"https://psp.com\", \"methodNames\":"
                                        + " [\"https://company.com/pay\"], \"methodData\":"
                                        + " [{\"https://company.com/pay\": null}]}"));
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testSuccessfulPayment() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        invokePaymentApp(app, Activity.RESULT_OK);
        Assert.assertNull(mErrorMessage);
        Assert.assertEquals("https://company.com/pay", mPaymentMethodName);
        Assert.assertEquals("{}", mPaymentDetails);
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testCancelledPayment() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        invokePaymentApp(app, Activity.RESULT_CANCELED);
        Assert.assertEquals(
                "Payment app returned RESULT_CANCELED code. This is how payment apps "
                        + "can close their activity programmatically.",
                mErrorMessage);
        Assert.assertNull(mPaymentMethodName);
        Assert.assertNull(mPaymentDetails);
    }

    private AndroidPaymentApp createApp(boolean showReadyToPayDebugInfo) {
        AndroidPaymentApp app =
                new AndroidPaymentApp(
                        mLauncherMock,
                        "com.company.app",
                        "com.company.app.PaymentActivity",
                        "com.company.app.IsReadyToPayService",
                        "App Label",
                        /* icon= */ null,
                        /* isIncognito= */ false,
                        /* appToHide= */ null,
                        new SupportedDelegations(),
                        showReadyToPayDebugInfo);
        app.addMethodName("https://company.com/pay");
        return app;
    }

    private void queryReadyToPay(AndroidPaymentApp app) throws Exception {
        app.bypassIsReadyToPayServiceInTest();
        app.maybeQueryIsReadyToPayService(
                mMethods,
                "https://merchant.com",
                "https://psp.com",
                /* certificateChain= */ null,
                /* modifiers= */ new HashMap<String, PaymentDetailsModifier>(),
                new AndroidPaymentApp.IsReadyToPayCallback() {
                    @Override
                    public void onIsReadyToPayResponse(
                            AndroidPaymentApp app, boolean isReadyToPay) {
                        mReadyToPayQueryFinished = true;
                        mReadyToPayResponse = isReadyToPay;
                    }
                });
        CriteriaHelper.pollUiThreadNested(() -> mReadyToPayQueryFinished);
        Assert.assertTrue("Payment app should be ready to pay", mReadyToPayResponse);
    }

    private void invokePaymentApp(AndroidPaymentApp app, int resultCode) throws Exception {
        PaymentItem total = new PaymentItem();
        total.amount = new PaymentCurrencyAmount();
        total.amount.currency = "USD";
        total.amount.value = "1.00";
        total.label = "Total";
        app.invokePaymentApp(
                "request-id",
                "Merchant Name",
                "https://merchant.com",
                "https://psp.com",
                /* certificateChain= */ null,
                mMethods,
                total,
                /* displayItems= */ new ArrayList<PaymentItem>(),
                /* modifiers= */ new HashMap<String, PaymentDetailsModifier>(),
                new PaymentOptions(),
                new ArrayList<PaymentShippingOption>(),
                new PaymentApp.InstrumentDetailsCallback() {
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
