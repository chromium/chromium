// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.mockito.Mockito;

import org.chromium.components.payments.PaymentRequestService.Delegate;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.url.Origin;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/** A builder of PaymentRequestService for testing. */
public class PaymentRequestServiceBuilder implements PaymentRequestService.Delegate {
    private static final String TWA_PACKAGE_NAME = "twa.package.name";
    private final Delegate mDelegate;
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
    private boolean mGooglePayBridgeEligible;
    private boolean mPrefsCanMakePayment;
    private String mInvalidSslCertificateErrorMessage;
    private boolean mIsOriginSecure = true;
    private JourneyLogger mJourneyLogger;
    private String mTopLevelOrigin;
    private String mFrameOrigin;
    private boolean mIsOriginAllowedToUseWebPaymentApis = true;
    private boolean mIsPaymentDetailsValid = true;
    private PaymentRequestSpec mSpec;

    /* package */ static PaymentRequestServiceBuilder defaultBuilder(Runnable onClosedListener,
            PaymentRequestClient client, PaymentAppService appService,
            BrowserPaymentRequest browserPaymentRequest, JourneyLogger journeyLogger) {
        return new PaymentRequestServiceBuilder(
                onClosedListener, client, appService, browserPaymentRequest, journeyLogger);
    }

    private PaymentRequestServiceBuilder(Runnable onClosedListener, PaymentRequestClient client,
            PaymentAppService appService, BrowserPaymentRequest browserPaymentRequest,
            JourneyLogger journeyLogger) {
        mDelegate = this;
        mWebContents = Mockito.mock(WebContents.class);
        setTopLevelOrigin("https://top.level.origin");
        mRenderFrameHost = Mockito.mock(RenderFrameHost.class);
        setFrameOrigin("https://frame.origin");
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
    public boolean isOriginSecure(String url) {
        return mIsOriginSecure;
    }

    @Override
    public JourneyLogger createJourneyLogger(boolean isIncognito, WebContents webContents) {
        return mJourneyLogger;
    }

    @Override
    public String formatUrlForSecurityDisplay(String url) {
        return url;
    }

    @Override
    public byte[][] getCertificateChain(WebContents webContents) {
        return new byte[0][];
    }

    @Override
    public boolean isOriginAllowedToUseWebPaymentApis(String lastCommittedUrl) {
        return mIsOriginAllowedToUseWebPaymentApis;
    }

    @Override
    public boolean validatePaymentDetails(PaymentDetails details) {
        return mIsPaymentDetailsValid;
    }

    @Override
    public PaymentRequestSpec createPaymentRequestSpec(PaymentOptions paymentOptions,
            PaymentDetails details, Collection<PaymentMethodData> values,
            String defaultLocaleString) {
        return mSpec;
    }

    @Override
    public PaymentAppService getPaymentAppService() {
        return mPaymentAppService;
    }

    /* package */ PaymentRequestServiceBuilder setRenderFrameHostLastCommittedOrigin(
            Origin origin) {
        Mockito.doReturn(origin).when(mRenderFrameHost).getLastCommittedOrigin();
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setRenderFrameHostLastCommittedURL(String url) {
        Mockito.doReturn(url).when(mRenderFrameHost).getLastCommittedURL();
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setOffTheRecord(boolean isOffTheRecord) {
        mIsOffTheRecord = isOffTheRecord;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setOriginSecure(boolean isSecure) {
        mIsOriginSecure = isSecure;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setJourneyLogger(JourneyLogger journeyLogger) {
        mJourneyLogger = journeyLogger;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setPaymentRequestClient(
            PaymentRequestClient client) {
        mClient = client;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setMethodData(PaymentMethodData[] methodData) {
        mMethodData = methodData;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setPaymentDetailsInit(PaymentDetails details) {
        mDetails = details;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setPaymentDetailsInitId(String id) {
        mDetails.id = id;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setPaymentDetailsInitTotal(PaymentItem total) {
        mDetails.total = total;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setOptions(PaymentOptions options) {
        mOptions = options;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setGooglePayBridgeEligible(boolean eligible) {
        mGooglePayBridgeEligible = eligible;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setWebContents(WebContents webContents) {
        mWebContents = webContents;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setTopLevelOrigin(String topLevelOrigin) {
        Mockito.doReturn(topLevelOrigin).when(mWebContents).getLastCommittedUrl();
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setFrameOrigin(String frameOrigin) {
        Mockito.doReturn(frameOrigin).when(mRenderFrameHost).getLastCommittedURL();
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setInvalidSslCertificateErrorMessage(
            String invalidSslCertificateErrorMessage) {
        mInvalidSslCertificateErrorMessage = invalidSslCertificateErrorMessage;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setOriginAllowedToUseWebPaymentApis(
            boolean isAllowed) {
        mIsOriginAllowedToUseWebPaymentApis = isAllowed;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setIsPaymentDetailsValid(boolean isValid) {
        mIsPaymentDetailsValid = isValid;
        return this;
    }

    /* package */ PaymentRequestServiceBuilder setPaymentRequestSpec(PaymentRequestSpec spec) {
        mSpec = spec;
        return this;
    }

    /* package */ PaymentRequestService build() {
        PaymentRequestService service =
                new PaymentRequestService(mRenderFrameHost, mClient, mOnClosedListener, mDelegate);
        boolean success = service.init(mMethodData, mDetails, mOptions, mGooglePayBridgeEligible);
        return success ? service : null;
    }
}
