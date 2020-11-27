// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;

/** A test for PaymentRequestService. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PaymentRequestServiceTest implements PaymentRequestClient {
    private static final int NO_PAYMENT_ERROR = PaymentErrorReason.MIN_VALUE;
    private final BrowserPaymentRequest mBrowserPaymentRequest;
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    private boolean mIsOnCloseListenerInvoked;
    private String mSentMethodName;
    private String mSentStringifiedDetails;
    private PaymentAddress mSentAddress;
    private String mSentShippingOptionId;
    private PayerDetail mSentPayerDetail;
    private PaymentResponse mSentPaymentResponse;
    private int mSentErrorReason;
    private String mSentErrorMessage;
    private boolean mOnCompleteCalled;
    private boolean mIsAbortedSuccessfully;
    private int mSentCanMakePayment;
    private int mSentHasEnrolledInstrument;
    private boolean mWarnNoFaviconCalled;
    private boolean mIsClientClosed;
    private MojoException mConnectionError;

    public PaymentRequestServiceTest() {
        mBrowserPaymentRequest = Mockito.mock(BrowserPaymentRequest.class);
    }

    @Override
    public void onPaymentMethodChange(String methodName, String stringifiedDetails) {
        mSentMethodName = methodName;
        mSentStringifiedDetails = stringifiedDetails;
    }

    @Override
    public void onShippingAddressChange(PaymentAddress address) {
        mSentAddress = address;
    }

    @Override
    public void onShippingOptionChange(String shippingOptionId) {
        mSentShippingOptionId = shippingOptionId;
    }

    @Override
    public void onPayerDetailChange(PayerDetail detail) {
        mSentPayerDetail = detail;
    }

    @Override
    public void onPaymentResponse(PaymentResponse response) {
        mSentPaymentResponse = response;
    }

    @Override
    public void onError(int error, String errorMessage) {
        mSentErrorReason = error;
        mSentErrorMessage = errorMessage;
    }

    @Override
    public void onComplete() {
        mOnCompleteCalled = true;
    }

    @Override
    public void onAbort(boolean abortedSuccessfully) {
        mIsAbortedSuccessfully = abortedSuccessfully;
    }

    @Override
    public void onCanMakePayment(int result) {
        mSentCanMakePayment = result;
    }

    @Override
    public void onHasEnrolledInstrument(int result) {
        mSentHasEnrolledInstrument = result;
    }

    @Override
    public void warnNoFavicon() {
        mWarnNoFaviconCalled = true;
    }

    @Override
    public void close() {
        mIsClientClosed = true;
    }

    @Override
    public void onConnectionError(MojoException e) {
        mConnectionError = e;
    }

    private void assertNoError() {
        assertErrorAndReason(null, NO_PAYMENT_ERROR);
    }

    private void assertErrorAndReason(String errorMessage, int errorReason) {
        Assert.assertEquals(errorMessage, mSentErrorMessage);
        Assert.assertEquals(errorReason, mSentErrorReason);
    }

    private PaymentRequestServiceBuilder defaultBuilder() {
        return PaymentRequestServiceBuilder.defaultBuilder(
                () -> mIsOnCloseListenerInvoked = true, /*client=*/this, mBrowserPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testNullFrameOriginFailsCreation() {
        Assert.assertNull(defaultBuilder().setRenderFrameHostLastCommittedOrigin(null).build());
        // Not asserting error because no frame to receive the error message and error reason.
    }

    @Test
    @Feature({"Payments"})
    public void testNullFrameUrlFailsCreation() {
        Assert.assertNull(defaultBuilder().setRenderFrameHostLastCommittedURL(null).build());
        // Not asserting error because no frame to receive the error message and error reason.
    }

    @Test
    @Feature({"Payments"})
    public void testNullWebContentsFailsCreation() {
        Assert.assertNull(defaultBuilder().setWebContents(null).build());
        // Not asserting error because no WebContents to receive the error message and error reason.
    }

    @Test
    @Feature({"Payments"})
    public void testDestroyedWebContentsFailsCreation() {
        WebContents webContents = Mockito.mock(WebContents.class);
        Mockito.doReturn(true).when(webContents).isDestroyed();
        Assert.assertNull(defaultBuilder().setWebContents(webContents).build());
        // Not asserting error because no WebContents to receive the error message and error reason.
    }

    @Test
    @Feature({"Payments"})
    public void testNullClientFailsCreation() {
        Assert.assertNull(defaultBuilder().setPaymentRequestClient(null).build());
        // Not asserting error because no client to receive the error message and error reason.
    }

    @Test
    @Feature({"Payments"})
    public void testInsecureOriginFailsCreation() {
        Assert.assertNull(defaultBuilder().setOriginSecure(false).build());
    }

    @Test
    @Feature({"Payments"})
    public void testNullMethodDataFailsCreation() {
        Assert.assertNull(defaultBuilder().setMethodData(null).build());
    }

    @Test
    @Feature({"Payments"})
    public void testNullDetailsFailsCreation() {
        Assert.assertNull(defaultBuilder().setDetails(null).build());
    }

    @Test
    @Feature({"Payments"})
    public void testNullOptionsFailsCreation() {
        Assert.assertNull(defaultBuilder().setOptions(null).build());
    }

    @Test
    @Feature({"Payments"})
    public void testSslErrorFailsCreation() {
        Assert.assertNull(
                defaultBuilder().setInvalidSslCertificateErrorMessage("StubbedError").build());
        assertErrorAndReason(
                "StubbedError", PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
    }

    @Test
    @Feature({"Payments"})
    public void testDisallowedOriginFailsCreation() {
        Assert.assertNull(defaultBuilder().setOriginAllowedToUseWebPaymentApis(false).build());
        assertErrorAndReason(ErrorStrings.PROHIBITED_ORIGIN,
                PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
    }

    @Test
    @Feature({"Payments"})
    public void testMethodDataNullElementFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
    }

    @Test
    @Feature({"Payments"})
    public void testEmptyMethodNameFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        methodData[0] = new PaymentMethodData();
        methodData[0].supportedMethod = "";
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
    }

    @Test
    @Feature({"Payments"})
    public void testNullMethodNameFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        methodData[0] = new PaymentMethodData();
        methodData[0].supportedMethod = null;
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
    }

    @Test
    @Feature({"Payments"})
    public void testInvalidDetailsFailsCreation() {
        Assert.assertNull(defaultBuilder().setIsPaymentDetailsValid(false).build());
        assertErrorAndReason(ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
    }

    @Test
    @Feature({"Payments"})
    public void testNullRawTotalFailsCreation() {
        PaymentRequestSpec spec = Mockito.mock(PaymentRequestSpec.class);
        Mockito.doReturn(null).when(spec).getRawTotal();
        Assert.assertNull(defaultBuilder().setPaymentRequestSpec(spec).build());
        assertErrorAndReason(ErrorStrings.TOTAL_REQUIRED, PaymentErrorReason.USER_CANCEL);
    }

    @Test
    @Feature({"Payments"})
    public void testDefaultParamsMakeCreationSuccess() {
        PaymentRequestService service = defaultBuilder().build();
        Assert.assertNotNull(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1)).onSpecValidated(Mockito.notNull());
        assertNoError();
    }
}
