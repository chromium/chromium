// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests for the browser-global manager of the payment flow UI. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserGlobalPaymentFlowManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PaymentRequestService mPaymentRequestService;
    @Mock private PaymentRequestService mOtherPaymentRequestService;
    @Mock private AndroidPaymentApp mAndroidPaymentApp;
    @Mock private PaymentApp mNonAndroidPaymentApp;

    @Before
    public void setUp() {
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
        BrowserGlobalPaymentFlowManager.resetShowingPaymentFlowForTest();
    }

    @After
    public void tearDown() {
        BrowserGlobalPaymentFlowManager.resetShowingPaymentFlowForTest();
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
    }

    /** Test for not showing payment flow by default. */
    @Test
    @Feature({"Payments"})
    public void testNotShowingPaymentFlowByDefault() throws Exception {
        Assert.assertFalse(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
    }

    /** Test for starting a payment flow. */
    @Test
    @Feature({"Payments"})
    public void testShowingPaymentFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));

        Assert.assertTrue(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
        Assert.assertEquals(
                mPaymentRequestService, BrowserGlobalPaymentFlowManager.getShowingPaymentFlow());
    }

    /** Test that starting a second payment flow is not possible. */
    @Test
    @Feature({"Payments"})
    public void testNoShowSecondPaymentFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));

        Assert.assertFalse(
                BrowserGlobalPaymentFlowManager.startPaymentFlow(mOtherPaymentRequestService));

        Assert.assertTrue(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
        Assert.assertEquals(
                mPaymentRequestService, BrowserGlobalPaymentFlowManager.getShowingPaymentFlow());
    }

    /** Test for stopping a payment flow. */
    @Test
    @Feature({"Payments"})
    public void testStoppingPaymentFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));

        BrowserGlobalPaymentFlowManager.onPaymentFlowStopped(mPaymentRequestService);

        Assert.assertFalse(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
        Assert.assertNull(BrowserGlobalPaymentFlowManager.getShowingPaymentFlow());
    }

    /** Test for starting a second payment flow after stopping the first one. */
    @Test
    @Feature({"Payments"})
    public void testStartSecondPaymentFlowAfterStoppingFirstOne() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        BrowserGlobalPaymentFlowManager.onPaymentFlowStopped(mPaymentRequestService);

        Assert.assertTrue(
                BrowserGlobalPaymentFlowManager.startPaymentFlow(mOtherPaymentRequestService));

        Assert.assertTrue(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
        Assert.assertEquals(
                mOtherPaymentRequestService,
                BrowserGlobalPaymentFlowManager.getShowingPaymentFlow());
    }

    /**
     * Test that stopping a non-showing payment flow will not change the currently showing payment
     * flow.
     */
    @Test
    @Feature({"Payments"})
    public void testOtherPaymentFlowDoesNotStopSHowingPaymentFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));

        BrowserGlobalPaymentFlowManager.onPaymentFlowStopped(mOtherPaymentRequestService);

        Assert.assertTrue(BrowserGlobalPaymentFlowManager.isShowingPaymentFlowForTest());
        Assert.assertEquals(
                mPaymentRequestService, BrowserGlobalPaymentFlowManager.getShowingPaymentFlow());
    }

    /**
     * Test for not being able to initialize the payment details update service helper outside of a
     * payment flow.
     */
    @Test
    @Feature({"Payments"})
    public void testNoInitPaymentDetailsUpdateServiceHelperOutsidePaymentFLow() throws Exception {
        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mAndroidPaymentApp);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /** Test for initializing payment details update service helper for the invoked payment app. */
    @Test
    @Feature({"Payments"})
    public void testInitPaymentDetailsUpdateServiceHelperForInvokedApp() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.NATIVE_MOBILE_APP)
                .when(mAndroidPaymentApp)
                .getPaymentAppType();
        Mockito.doReturn("example.test").when(mAndroidPaymentApp).packageName();

        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mAndroidPaymentApp);

        Assert.assertTrue(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /**
     * Test for non-showing payment flow not being able to initialize the payment details update
     * service helper.
     */
    @Test
    @Feature({"Payments"})
    public void testNoInitPaymentDetailsUpdateServiceHelperNonShownPaymentFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));

        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mOtherPaymentRequestService, mAndroidPaymentApp);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /** Test web payment handler does not initialize the payment details update service helper. */
    @Test
    @Feature({"Payments"})
    public void testNoInitPaymentDetailsUpdateServiceHelperWebPaymentHandler() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.SERVICE_WORKER_APP)
                .when(mAndroidPaymentApp)
                .getPaymentAppType();

        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mNonAndroidPaymentApp);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /** Test internal app does not initialize the payment details update service helper. */
    @Test
    @Feature({"Payments"})
    public void testNoInitPaymentDetailsUpdateServiceHelperInternalApp() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.INTERNAL).when(mAndroidPaymentApp).getPaymentAppType();

        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mNonAndroidPaymentApp);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /**
     * Test undefined payment app type does not initialize the payment details update service
     * helper.
     */
    @Test
    @Feature({"Payments"})
    public void testNoInitPaymentDetailsUpdateServiceHelperUndefinedAppType() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.UNDEFINED).when(mAndroidPaymentApp).getPaymentAppType();

        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mNonAndroidPaymentApp);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /**
     * Test for resetting the dynamic price update service helper for the invoked payment app, after
     * this app stops.
     */
    @Test
    @Feature({"Payments"})
    public void testOnInvokedPaymentAppStopped() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.NATIVE_MOBILE_APP)
                .when(mAndroidPaymentApp)
                .getPaymentAppType();
        Mockito.doReturn("example.test").when(mAndroidPaymentApp).packageName();
        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mAndroidPaymentApp);

        BrowserGlobalPaymentFlowManager.onInvokedPaymentAppStopped(mPaymentRequestService);

        Assert.assertFalse(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }

    /** Test for non-showing payment flow not being able to reset the update service helper. */
    @Test
    @Feature({"Payments"})
    public void testNoResetPaymentDetailsUpdateServiceHelperNonShowingPayFlow() throws Exception {
        Assert.assertTrue(BrowserGlobalPaymentFlowManager.startPaymentFlow(mPaymentRequestService));
        Mockito.doReturn(PaymentAppType.NATIVE_MOBILE_APP)
                .when(mAndroidPaymentApp)
                .getPaymentAppType();
        Mockito.doReturn("example.test").when(mAndroidPaymentApp).packageName();
        BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
                mPaymentRequestService, mAndroidPaymentApp);

        BrowserGlobalPaymentFlowManager.onInvokedPaymentAppStopped(mOtherPaymentRequestService);

        Assert.assertTrue(PaymentDetailsUpdateServiceHelper.isInitializedForTest());
    }
}
