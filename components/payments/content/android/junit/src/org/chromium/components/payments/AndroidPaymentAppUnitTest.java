// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentEventResponseType;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Tests for the Android intent-based payment app. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES,
    PaymentFeatureList.SURFACE_WALLET_ERROR_CODE_FROM_INTENT
})
public class AndroidPaymentAppUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AndroidIntentLauncher mLauncherMock;
    @Mock private DialogController mDialogControllerMock;

    private String mErrorMessage;
    private String mPaymentMethodName;
    private String mPaymentDetails;
    private boolean mReadyToPayResponse;
    private boolean mReadyToPayQueryFinished;
    private boolean mInvokePaymentAppFinished;
    private Map<String, PaymentMethodData> mMethods;

    @Before
    public void setUp() {
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

    @Test
    public void testNoReadyToPayDebugInfo() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        Mockito.verify(mDialogControllerMock, Mockito.never())
                .showReadyToPayDebugInfo(Mockito.any());
    }

    @Test
    public void testShowReadyToPayDebugInfo() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ true);
        queryReadyToPay(app);
        Mockito.verify(mDialogControllerMock, Mockito.times(1))
                .showReadyToPayDebugInfo(
                        Mockito.eq(
                                "IS_READY_TO_PAY sent to com.company.app.IsReadyToPayService in"
                                        + " com.company.app with {\"topLevelOrigin\":"
                                        + " \"https://merchant.com\", \"paymentRequestOrigin\":"
                                        + " \"https://psp.com\", \"methodNames\":"
                                        + " [\"https://company.com/pay\"], \"methodData\":"
                                        + " [{\"https://company.com/pay\": null}]}"));
    }

    @Test
    public void testSetHasEnrolledInstrument() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        Assert.assertFalse(app.hasEnrolledInstrument());
        app.setHasEnrolledInstrument(true);
        Assert.assertTrue(app.hasEnrolledInstrument());
    }

    @Test
    public void testSuccessfulPayment() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        invokePaymentApp(app, Activity.RESULT_OK);
        Assert.assertNull(mErrorMessage);
        Assert.assertEquals("https://company.com/pay", mPaymentMethodName);
        Assert.assertEquals("{}", mPaymentDetails);
    }

    @Test
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

    @Test
    public void testInternalAppErrorPayment() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        invokePaymentApp(app, WebPaymentIntentHelper.RESULT_INTERNAL_APP_ERROR);
        Assert.assertEquals(
                "Payment app returned RESULT_INTERNAL_APP_ERROR code. Native payment app"
                        + " encountered an internal app error.",
                mErrorMessage);
        Assert.assertNull(mPaymentMethodName);
        Assert.assertNull(mPaymentDetails);
    }

    @Test
    @EnableFeatures({PaymentFeatureList.SURFACE_WALLET_ERROR_CODE_FROM_INTENT})
    public void testInternalAppErrorPaymentWithWalletErrorCode() throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        Bundle extras = new Bundle();
        extras.putInt(WebPaymentIntentHelper.EXTRA_WALLET_ERROR_CODE, 409);
        invokePaymentApp(app, WebPaymentIntentHelper.RESULT_INTERNAL_APP_ERROR, extras);
        Assert.assertEquals(
                "Internal payment app returned error with status code: 409", mErrorMessage);
        Assert.assertNull(mPaymentMethodName);
        Assert.assertNull(mPaymentDetails);
    }

    @Test
    @DisableFeatures({PaymentFeatureList.SURFACE_WALLET_ERROR_CODE_FROM_INTENT})
    public void
            surfaceWalletErrorCodeFromIntentDisabled_testInternalAppErrorPaymentWithWalletErrorCode()
                    throws Exception {
        AndroidPaymentApp app = createApp(/* showReadyToPayDebugInfo= */ false);
        queryReadyToPay(app);
        Bundle extras = new Bundle();
        extras.putInt(WebPaymentIntentHelper.EXTRA_WALLET_ERROR_CODE, 409);
        invokePaymentApp(app, WebPaymentIntentHelper.RESULT_INTERNAL_APP_ERROR, extras);
        Assert.assertEquals(ErrorStrings.RESULT_INTERNAL_APP_ERROR, mErrorMessage);
        Assert.assertNull(mPaymentMethodName);
        Assert.assertNull(mPaymentDetails);
    }

    private AndroidPaymentApp createApp(boolean showReadyToPayDebugInfo) {
        AndroidPaymentApp app =
                new AndroidPaymentApp(
                        mLauncherMock,
                        mDialogControllerMock,
                        "com.company.app",
                        "com.company.app.PaymentActivity",
                        "com.company.app.IsReadyToPayService",
                        "com.company.app.PaymentDetailsUpdateService",
                        "App Label",
                        /* icon= */ null,
                        /* isIncognito= */ false,
                        /* appToHide= */ null,
                        new SupportedDelegations(),
                        showReadyToPayDebugInfo,
                        /* removeDeprecatedFields= */ false,
                        /* paymentDetailsUpdateServiceMaxRetryNumber= */ 0);
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
                /* modifiers= */ new HashMap<>(),
                new AndroidPaymentApp.IsReadyToPayCallback() {
                    @Override
                    public void onIsReadyToPayResponse(
                            AndroidPaymentApp app, boolean isReadyToPay) {
                        mReadyToPayQueryFinished = true;
                        mReadyToPayResponse = isReadyToPay;
                    }
                });
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(mReadyToPayQueryFinished);
        Assert.assertTrue("Payment app should be ready to pay", mReadyToPayResponse);
    }

    private void invokePaymentApp(AndroidPaymentApp app, int resultCode) throws Exception {
        invokePaymentApp(app, resultCode, new Bundle());
    }

    private void invokePaymentApp(AndroidPaymentApp app, int resultCode, Bundle additionalExtras)
            throws Exception {
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
                /* displayItems= */ new ArrayList<>(),
                /* modifiers= */ new HashMap<>(),
                new PaymentOptions(),
                new ArrayList<>(),
                new PaymentApp.InstrumentDetailsCallback() {
                    @Override
                    public void onInstrumentDetailsReady(
                            String methodName, String stringifiedDetails, PayerData payerData) {
                        mPaymentMethodName = methodName;
                        mPaymentDetails = stringifiedDetails;
                        mInvokePaymentAppFinished = true;
                    }

                    @Override
                    public void onInstrumentDetailsError(
                            @PaymentEventResponseType.EnumType int error, String errorMessage) {
                        mErrorMessage = errorMessage;
                        mInvokePaymentAppFinished = true;
                    }
                });
        Intent data = new Intent();
        Bundle extras = new Bundle();
        extras.putString("methodName", "https://company.com/pay");
        extras.putString("details", "{}");
        extras.putAll(additionalExtras);
        data.putExtras(extras);
        app.onIntentCompleted(resultCode, data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(mInvokePaymentAppFinished);
    }
}
