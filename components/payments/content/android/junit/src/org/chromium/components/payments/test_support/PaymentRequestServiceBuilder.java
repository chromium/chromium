// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import androidx.annotation.Nullable;

import org.mockito.Mockito;

import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.Delegate;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.SecurePaymentConfirmationRequest;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.Origin;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/** A builder of PaymentRequestService for testing. */
public class PaymentRequestServiceBuilder implements Delegate {
    private static final String TWA_PACKAGE_NAME = "twa.package.name";
    private final RenderFrameHost mRenderFrameHost;
    private final Runnable mOnClosedListener;
    private final PaymentAppService mPaymentAppService;
    private final BrowserPaymentRequest mBrowserPaymentRequest;
    private WebContents mWebContents;
    private boolean mIsOffTheRecord = true;
    private PaymentRequestClient mClient;
    private PaymentMethodData[] mMethodData;
    private PaymentDetails mDetails;
    private PaymentOptions mOptions;
    private boolean mPrefsCanMakePayment;
    private String mInvalidSslCertificateErrorMessage;
    private boolean mIsOriginSecure = true;
    private JourneyLogger mJourneyLogger;
    private String mTopLevelOrigin;
    private String mFrameOrigin;
    private boolean mIsOriginAllowedToUseWebPaymentApis = true;
    private boolean mIsPaymentDetailsValid = true;
    private PaymentRequestSpec mSpec;
    private SecurePaymentConfirmationRequest mSecurePaymentConfirmationRequest;

    public static PaymentRequestServiceBuilder defaultBuilder(
            Runnable onClosedListener,
            PaymentRequestClient client,
            PaymentAppService appService,
            BrowserPaymentRequest browserPaymentRequest,
            JourneyLogger journeyLogger) {
        return new PaymentRequestServiceBuilder(
                onClosedListener, client, appService, browserPaymentRequest, journeyLogger);
    }

    public PaymentRequestServiceBuilder(
            Runnable onClosedListener,
            PaymentRequestClient client,
            PaymentAppService appService,
            BrowserPaymentRequest browserPaymentRequest,
            JourneyLogger journeyLogger) {
        mWebContents = Mockito.mock(WebContents.class);
        setTopLevelOrigin(JUnitTestGURLs.URL_1);
        mRenderFrameHost = Mockito.mock(RenderFrameHost.class);
        setFrameOrigin(JUnitTestGURLs.URL_2);
        Origin origin = Mockito.mock(Origin.class);
        Mockito.doReturn(origin).when(mRenderFrameHost).getLastCommittedOrigin();
        mJourneyLogger = journeyLogger;
        mMethodData = new PaymentMethodData[1];
        mMethodData[0] = new PaymentMethodData();
        mMethodData[0].supportedMethod = "https://www.chromium.org";
        mDetails = new PaymentDetails();
        mDetails.id = "testId";
        mDetails.total = new PaymentItem();
        mOptions = new PaymentOptions();
        mOptions.requestShipping = true;
        mSpec = Mockito.mock(PaymentRequestSpec.class);
        PaymentCurrencyAmount amount = new PaymentCurrencyAmount();
        amount.currency = "CNY";
        amount.value = "123";
        PaymentItem total = new PaymentItem();
        total.amount = amount;
        Mockito.doReturn(total).when(mSpec).getRawTotal();
        Map<String, PaymentMethodData> methodDataMap = new HashMap<>();
        Mockito.doReturn(methodDataMap).when(mSpec).getMethodData();
        mBrowserPaymentRequest = browserPaymentRequest;
        mOnClosedListener = onClosedListener;
        mClient = client;
        mPaymentAppService = appService;
        mSecurePaymentConfirmationRequest = new SecurePaymentConfirmationRequest();
        org.chromium.url.internal.mojom.Origin payeeOrigin =
                new org.chromium.url.internal.mojom.Origin();
        payeeOrigin.scheme = "https";
        payeeOrigin.host = "chromium.org";
        payeeOrigin.port = 443;
        mSecurePaymentConfirmationRequest.payeeOrigin = payeeOrigin;
    }

    @Override
    public BrowserPaymentRequest createBrowserPaymentRequest(
            PaymentRequestService paymentRequestService) {
        return mBrowserPaymentRequest;
    }

    @Override
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    @Override
    public String getInvalidSslCertificateErrorMessage() {
        return mInvalidSslCertificateErrorMessage;
    }

    @Override
    public boolean prefsCanMakePayment() {
        return mPrefsCanMakePayment;
    }

    @Nullable
    @Override
    public String getTwaPackageName() {
        return TWA_PACKAGE_NAME;
    }

    @Override
    public WebContents getLiveWebContents(RenderFrameHost renderFrameHost) {
        return mWebContents;
    }

    @Override
    public boolean isOriginSecure(GURL url) {
        return mIsOriginSecure;
    }

    @Override
    public JourneyLogger createJourneyLogger(WebContents webContents) {
        return mJourneyLogger;
    }

    @Override
    public String formatUrlForSecurityDisplay(GURL url) {
        return url.getSpec();
    }

    @Override
    public byte[][] getCertificateChain(WebContents webContents) {
        return new byte[0][];
    }

    @Override
    public boolean isOriginAllowedToUseWebPaymentApis(GURL lastCommittedUrl) {
        return mIsOriginAllowedToUseWebPaymentApis;
    }

    @Override
    public boolean validatePaymentDetails(PaymentDetails details) {
        return mIsPaymentDetailsValid;
    }

    @Override
    public PaymentRequestSpec createPaymentRequestSpec(
            PaymentOptions paymentOptions,
            PaymentDetails details,
            Collection<PaymentMethodData> values,
            String defaultLocaleString) {
        return mSpec;
    }

    @Override
    public PaymentAppService getPaymentAppService() {
        return mPaymentAppService;
    }

    public PaymentRequestServiceBuilder setRenderFrameHostLastCommittedOrigin(Origin origin) {
        Mockito.doReturn(origin).when(mRenderFrameHost).getLastCommittedOrigin();
        return this;
    }

    public PaymentRequestServiceBuilder setRenderFrameHostLastCommittedURL(GURL url) {
        Mockito.doReturn(url).when(mRenderFrameHost).getLastCommittedURL();
        return this;
    }

    public PaymentRequestServiceBuilder setOffTheRecord(boolean isOffTheRecord) {
        mIsOffTheRecord = isOffTheRecord;
        return this;
    }

    public PaymentRequestServiceBuilder setOriginSecure(boolean isSecure) {
        mIsOriginSecure = isSecure;
        return this;
    }

    public PaymentRequestServiceBuilder setJourneyLogger(JourneyLogger journeyLogger) {
        mJourneyLogger = journeyLogger;
        return this;
    }

    public PaymentRequestServiceBuilder setPaymentRequestClient(PaymentRequestClient client) {
        mClient = client;
        return this;
    }

    public PaymentRequestServiceBuilder setMethodData(PaymentMethodData[] methodData) {
        mMethodData = methodData;
        return this;
    }

    public PaymentRequestServiceBuilder setPaymentDetailsInit(PaymentDetails details) {
        mDetails = details;
        return this;
    }

    public PaymentRequestServiceBuilder setPaymentDetailsInitId(String id) {
        mDetails.id = id;
        return this;
    }

    public PaymentRequestServiceBuilder setPaymentDetailsInitTotal(PaymentItem total) {
        mDetails.total = total;
        return this;
    }

    public PaymentRequestServiceBuilder setOptions(PaymentOptions options) {
        mOptions = options;
        return this;
    }

    public PaymentRequestServiceBuilder setOnlySpcMethodWithoutPaymentOptions() {
        mOptions = new PaymentOptions();
        mMethodData = new PaymentMethodData[1];
        mMethodData[0] = new PaymentMethodData();
        mMethodData[0].supportedMethod = MethodStrings.SECURE_PAYMENT_CONFIRMATION;

        mMethodData[0].securePaymentConfirmation = mSecurePaymentConfirmationRequest;
        return this;
    }

    public PaymentRequestServiceBuilder setPayeeName(String payeeName) {
        mSecurePaymentConfirmationRequest.payeeName = payeeName;
        return this;
    }

    public PaymentRequestServiceBuilder setPayeeOrigin(
            org.chromium.url.internal.mojom.Origin payeeOrigin) {
        mSecurePaymentConfirmationRequest.payeeOrigin = payeeOrigin;
        return this;
    }

    public PaymentRequestServiceBuilder setWebContents(WebContents webContents) {
        mWebContents = webContents;
        return this;
    }

    public PaymentRequestServiceBuilder setTopLevelOrigin(GURL topLevelOrigin) {
        Mockito.doReturn(topLevelOrigin).when(mWebContents).getLastCommittedUrl();
        return this;
    }

    public PaymentRequestServiceBuilder setFrameOrigin(GURL frameOrigin) {
        Mockito.doReturn(frameOrigin).when(mRenderFrameHost).getLastCommittedURL();
        return this;
    }

    public PaymentRequestServiceBuilder setInvalidSslCertificateErrorMessage(
            String invalidSslCertificateErrorMessage) {
        mInvalidSslCertificateErrorMessage = invalidSslCertificateErrorMessage;
        return this;
    }

    public PaymentRequestServiceBuilder setOriginAllowedToUseWebPaymentApis(boolean isAllowed) {
        mIsOriginAllowedToUseWebPaymentApis = isAllowed;
        return this;
    }

    public PaymentRequestServiceBuilder setIsPaymentDetailsValid(boolean isValid) {
        mIsPaymentDetailsValid = isValid;
        return this;
    }

    public PaymentRequestServiceBuilder setPaymentRequestSpec(PaymentRequestSpec spec) {
        mSpec = spec;
        return this;
    }

    public PaymentRequestService build() {
        PaymentRequestService service =
                new PaymentRequestService(
                        mRenderFrameHost,
                        mClient,
                        mOnClosedListener,
                        /* delegate= */ this,
                        () -> null);
        boolean success = service.init(mMethodData, mDetails, mOptions);
        return success ? service : null;
    }
}
