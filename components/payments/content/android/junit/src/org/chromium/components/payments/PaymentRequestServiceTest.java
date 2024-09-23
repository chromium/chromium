// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.payments.test_support.DefaultPaymentFeatureConfig;
import org.chromium.components.payments.test_support.PaymentRequestServiceBuilder;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImplJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.url.mojom.Url;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A test for PaymentRequestService. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES)
public class PaymentRequestServiceTest implements PaymentRequestClient {
    private static final int NATIVE_WEB_CONTENTS_ANDROID = 1;
    private static final int NO_PAYMENT_ERROR = PaymentErrorReason.MIN_VALUE;
    private final BrowserPaymentRequest mBrowserPaymentRequest;
    private List<PaymentApp> mNotifiedPendingApps;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Mock private PaymentRequestWebContentsData.Natives mWebContentsDataJniMock;

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
    private boolean mWaitForUpdatedDetailsDefaultValue;
    private boolean mIsUserGestureShow;
    private PaymentAppService mPaymentAppService;
    private PaymentAppFactoryDelegate mPaymentAppFactoryDelegate;
    private JourneyLogger mJourneyLogger;
    private PaymentRequestWebContentsData mPaymentRequestWebContentsData;

    public PaymentRequestServiceTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebContentsImplJni.TEST_HOOKS, mWebContentsJniMock);
        WebContentsImpl webContentsImpl =
                Mockito.spy(
                        WebContentsImpl.create(NATIVE_WEB_CONTENTS_ANDROID, mNavigationController));
        // We don't mock the WebContentsObserverProxy, so mock the observer behaviour.
        Mockito.doNothing().when(webContentsImpl).addObserver(Mockito.any());
        webContentsImpl.initializeForTesting();
        mPaymentRequestWebContentsData = new PaymentRequestWebContentsData(webContentsImpl);
        PaymentRequestWebContentsData.setInstanceForTesting(mPaymentRequestWebContentsData);

        mJniMocker.mock(PaymentRequestWebContentsDataJni.TEST_HOOKS, mWebContentsDataJniMock);
        Mockito.doNothing().when(mWebContentsDataJniMock).recordActivationlessShow(Mockito.any());
        Mockito.doReturn(false).when(mWebContentsDataJniMock).hadActivationlessShow(Mockito.any());

        mPaymentAppService = Mockito.mock(PaymentAppService.class);
        Mockito.doAnswer(
                        (args) -> {
                            mPaymentAppFactoryDelegate = args.getArgument(0);
                            return null;
                        })
                .when(mPaymentAppService)
                .create(Mockito.any());

        mBrowserPaymentRequest = Mockito.mock(BrowserPaymentRequest.class);
        Mockito.doReturn(true).when(mBrowserPaymentRequest).hasAvailableApps();
        Mockito.doReturn(false)
                .when(mBrowserPaymentRequest)
                .disconnectIfExtraValidationFails(
                        Mockito.any(), Mockito.any(), Mockito.any(), Mockito.any());
        Mockito.doReturn(true)
                .when(mBrowserPaymentRequest)
                .patchPaymentResponseIfNeeded(Mockito.any());
        Mockito.doReturn(null)
                .when(mBrowserPaymentRequest)
                .showOrSkipAppSelector(Mockito.anyBoolean(), Mockito.any(), Mockito.anyBoolean());
        Mockito.doReturn(true).when(mBrowserPaymentRequest).onPaymentAppCreated(Mockito.any());
        Mockito.doReturn(true)
                .when(mBrowserPaymentRequest)
                .parseAndValidateDetailsFurtherIfNeeded(Mockito.any());
        Mockito.doAnswer(
                        (args) -> {
                            List<PaymentApp> pendingApps = args.getArgument(0);
                            mNotifiedPendingApps = new ArrayList<>(pendingApps);
                            return null;
                        })
                .when(mBrowserPaymentRequest)
                .notifyPaymentUiOfPendingApps(Mockito.any());

        PaymentApp app =
                new PaymentApp("appId", "appLabel", "appSublabel", Mockito.mock(Drawable.class)) {
                    @Override
                    public Set<String> getInstrumentMethodNames() {
                        Set<String> names = new HashSet<>();
                        names.add("https://www.chromium.org");
                        return names;
                    }

                    @Override
                    public void dismissInstrument() {}
                };
        Mockito.doReturn(app).when(mBrowserPaymentRequest).getSelectedPaymentApp();
        List<PaymentApp> apps = new ArrayList();
        apps.add(app);
        Mockito.doReturn(apps).when(mBrowserPaymentRequest).getPaymentApps();

        mJourneyLogger = Mockito.mock(JourneyLogger.class);
    }

    @Before
    public void setUp() {
        PaymentRequestService.resetShowingPaymentRequestForTest();
        DefaultPaymentFeatureConfig.setDefaultFlagConfigurationForTesting();
    }

    @After
    public void tearDown() {
        PaymentRequestService.resetShowingPaymentRequestForTest();
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
    public void allowConnectToSource(
            Url url,
            Url urlBeforeRedirects,
            boolean didFollowRedirect,
            AllowConnectToSource_Response callback) {
        callback.call(/* allow= */ true);
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

    private void assertClosed(boolean isClosed) {
        Assert.assertEquals(mIsClientClosed, isClosed);
        Assert.assertEquals(mIsOnCloseListenerInvoked, isClosed);
    }

    private void show(PaymentRequestService service) {
        service.show(mWaitForUpdatedDetailsDefaultValue, mIsUserGestureShow);
    }

    private void updateWith(PaymentRequestService service) {
        service.updateWith(getDefaultPaymentDetailsUpdate());
    }

    private PaymentRequestServiceBuilder defaultBuilder() {
        return PaymentRequestServiceBuilder.defaultBuilder(
                () -> mIsOnCloseListenerInvoked = true,
                /* client= */ this,
                mPaymentAppService,
                mBrowserPaymentRequest,
                mJourneyLogger);
    }

    private PaymentApp createDefaultPaymentApp() {
        PaymentApp app = Mockito.mock(PaymentApp.class);
        Mockito.doReturn(true).when(app).hasEnrolledInstrument();
        return app;
    }

    private void queryPaymentApps() {
        mPaymentAppFactoryDelegate.onCanMakePaymentCalculated(true);
        mPaymentAppFactoryDelegate.onPaymentAppCreated(createDefaultPaymentApp());
        mPaymentAppFactoryDelegate.onDoneCreatingPaymentApps(null);
    }

    private PaymentDetails getDefaultPaymentDetailsUpdate() {
        return new PaymentDetails();
    }

    private void verifyShowAppSelector(int times) {
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(times))
                .showOrSkipAppSelector(Mockito.anyBoolean(), Mockito.any(), Mockito.anyBoolean());
    }

    private void verifyContinuedShowWithUpdatedDetails(int times) {
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(times))
                .continueShowWithUpdatedDetails(Mockito.any(), Mockito.anyBoolean());
    }

    private void resetErrorMessageAndCloseState() {
        mSentErrorReason = NO_PAYMENT_ERROR;
        mSentErrorMessage = null;
        mIsClientClosed = false;
        mIsOnCloseListenerInvoked = false;
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
        assertErrorAndReason(
                ErrorStrings.NOT_IN_A_SECURE_ORIGIN, PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testNullMethodDataFailsCreation() {
        Assert.assertNull(defaultBuilder().setMethodData(null).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testNullDetailsFailsCreation() {
        Assert.assertNull(defaultBuilder().setPaymentDetailsInit(null).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testDetailsWithoutIdFailsCreation() {
        Assert.assertNull(defaultBuilder().setPaymentDetailsInitId(null).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testDetailsWithoutTotalFailsCreation() {
        Assert.assertNull(defaultBuilder().setPaymentDetailsInitTotal(null).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testNullDetailsFailsUpdateWith() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(false, mIsUserGestureShow);
        assertNoError();
        service.updateWith(null);
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onPaymentDetailsUpdated(Mockito.any(), Mockito.anyBoolean());
    }

    @Test
    @Feature({"Payments"})
    public void testDetailsWithIdFailsUpdateWith() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(false, mIsUserGestureShow);
        PaymentDetails details = getDefaultPaymentDetailsUpdate();
        details.id = "testId";
        assertNoError();
        service.updateWith(details);
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onPaymentDetailsUpdated(Mockito.any(), Mockito.anyBoolean());
    }

    @Test
    @Feature({"Payments"})
    public void testOnPaymentDetailsUpdatedIsInvoked() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(false, mIsUserGestureShow);
        updateWith(service);
        assertNoError();
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1))
                .onPaymentDetailsUpdated(Mockito.any(), Mockito.anyBoolean());
    }

    @Test
    @Feature({"Payments"})
    public void testNullDetailsFailsContinueShow() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(true, mIsUserGestureShow);
        assertNoError();
        service.updateWith(null);
        assertErrorAndReason(ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
        verifyContinuedShowWithUpdatedDetails(0);
    }

    @Test
    @Feature({"Payments"})
    public void testDetailsWithIdFailsContinueShow() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(true, mIsUserGestureShow);
        assertNoError();
        PaymentDetails details = getDefaultPaymentDetailsUpdate();
        details.id = "testId";
        service.updateWith(details);
        assertErrorAndReason(ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
        verifyContinuedShowWithUpdatedDetails(0);
    }

    @Test
    @Feature({"Payments"})
    public void testContinueShowIsInvoked() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(true, mIsUserGestureShow);
        updateWith(service);
        assertNoError();
        verifyContinuedShowWithUpdatedDetails(1);
    }

    @Test
    @Feature({"Payments"})
    public void testCallUpdateWithBeforeShowFailsPayment() {
        PaymentRequestService service = defaultBuilder().build();
        assertNoError();

        updateWith(service);
        assertErrorAndReason(
                ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW, PaymentErrorReason.USER_CANCEL);
    }

    @Test
    @Feature({"Payments"})
    public void testCallUpdateWithWithoutRequestingAnyInfoFailsPayment() {
        PaymentRequestService service = defaultBuilder().setOptions(new PaymentOptions()).build();
        assertNoError();

        show(service);
        assertNoError();

        updateWith(service);
        assertErrorAndReason(ErrorStrings.INVALID_STATE, PaymentErrorReason.USER_CANCEL);
    }

    @Test
    @Feature({"Payments"})
    public void testCallOnPaymentDetailsNotUpdatedBeforeShowFailsPayment() {
        PaymentRequestService service = defaultBuilder().build();
        assertNoError();

        service.onPaymentDetailsNotUpdated();
        assertErrorAndReason(
                ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW, PaymentErrorReason.USER_CANCEL);
    }

    @Test
    @Feature({"Payments"})
    public void testOnConnectionErrorFailsPayment() {
        PaymentRequestService service = defaultBuilder().build();
        Assert.assertFalse(mIsOnCloseListenerInvoked);

        service.onConnectionError(null);
        Assert.assertTrue(mIsOnCloseListenerInvoked);
    }

    @Test
    @Feature({"Payments"})
    public void testNullOptionsFailsCreation() {
        Assert.assertNull(defaultBuilder().setOptions(null).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_OPTIONS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
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
        assertErrorAndReason(
                ErrorStrings.PROHIBITED_ORIGIN,
                PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
    }

    @Test
    @Feature({"Payments"})
    public void testMethodDataNullElementFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testEmptyMethodNameFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        methodData[0] = new PaymentMethodData();
        methodData[0].supportedMethod = "";
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testNullMethodNameFailsCreation() {
        PaymentMethodData[] methodData = new PaymentMethodData[1];
        methodData[0] = new PaymentMethodData();
        methodData[0].supportedMethod = null;
        Assert.assertNull(defaultBuilder().setMethodData(methodData).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testInvalidDetailsFailsCreation() {
        Assert.assertNull(defaultBuilder().setIsPaymentDetailsValid(false).build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_DETAILS,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testNullRawTotalFailsCreation() {
        PaymentRequestSpec spec = Mockito.mock(PaymentRequestSpec.class);
        Mockito.doReturn(null).when(spec).getRawTotal();
        Assert.assertNull(defaultBuilder().setPaymentRequestSpec(spec).build());
        assertErrorAndReason(
                ErrorStrings.TOTAL_REQUIRED, PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testTwoShowsFailService() {
        PaymentRequestService service = defaultBuilder().build();
        show(service);
        assertErrorAndReason(null, NO_PAYMENT_ERROR);
        assertClosed(false);
        show(service);
        assertErrorAndReason(ErrorStrings.CANNOT_SHOW_TWICE, PaymentErrorReason.USER_CANCEL);
        assertClosed(true);
    }

    @Test
    @Feature({"Payments"})
    public void testDefaultParamsMakeCreationSuccess() {
        Assert.assertNull(mPaymentAppFactoryDelegate);
        PaymentRequestService service = defaultBuilder().build();
        Assert.assertNotNull(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1)).onSpecValidated(Mockito.notNull());
        assertNoError();
        Assert.assertNotNull(mPaymentAppFactoryDelegate);
    }

    @Test
    @Feature({"Payments"})
    public void testCanNotMakePaymentFailsPayment() {
        PaymentRequestService service = defaultBuilder().build();
        show(service);
        mPaymentAppFactoryDelegate.onCanMakePaymentCalculated(false);
        mPaymentAppFactoryDelegate.onPaymentAppCreated(createDefaultPaymentApp());
        mPaymentAppFactoryDelegate.onDoneCreatingPaymentApps(null);
        assertErrorAndReason(ErrorStrings.USER_CANCELLED, PaymentErrorReason.USER_CANCEL);
        assertClosed(true);
    }

    @Test
    @Feature({"Payments"})
    public void testNoPaymentAppFailsPayment() {
        PaymentRequestService service = defaultBuilder().build();
        show(service);
        mPaymentAppFactoryDelegate.onDoneCreatingPaymentApps(null);
        assertErrorAndReason(ErrorStrings.USER_CANCELLED, PaymentErrorReason.USER_CANCEL);
        assertClosed(true);
    }

    @Test
    @Feature({"Payments"})
    public void testAppSelectorIsTriggeredOnShownAndAppsQueried() {
        PaymentRequestService service = defaultBuilder().build();
        verifyShowAppSelector(0);
        show(service);
        verifyShowAppSelector(0);
        queryPaymentApps();
        verifyShowAppSelector(1);
    }

    @Test
    @Feature({"Payments"})
    public void testUiIsNotifiedOfPendingAppsOnShownAndAppsQueried() {
        PaymentRequestService service = defaultBuilder().build();
        show(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .notifyPaymentUiOfPendingApps(Mockito.any());
        Assert.assertNull(mNotifiedPendingApps);
        queryPaymentApps();
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1))
                .notifyPaymentUiOfPendingApps(Mockito.any());
        Assert.assertEquals(1, mNotifiedPendingApps.size());
    }

    @Test
    @Feature({"Payments"})
    public void testAppSelectorIsNotTriggeredOnAppsQueriedOnly() {
        PaymentRequestService service = defaultBuilder().build();
        queryPaymentApps();
        verifyShowAppSelector(0);
        show(service);
        verifyShowAppSelector(1);
    }

    @Test
    @Feature({"Payments"})
    public void testInvokeUiSkipMethodOnShownAndAppsQueried() {
        PaymentRequestService service = defaultBuilder().build();
        show(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
        queryPaymentApps();
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1))
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
    }

    @Test
    @Feature({"Payments"})
    public void testWaitingForUpdatedDetailsDeterUiSkipMethod() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(true, mIsUserGestureShow);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
        queryPaymentApps();
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
        updateWith(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1))
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
    }

    @Test
    @Feature({"Payments"})
    public void testQueryFinishCanTriggerUiSkipped() {
        PaymentRequestService service = defaultBuilder().build();
        service.show(true, mIsUserGestureShow);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
        updateWith(service);
        Mockito.verify(mBrowserPaymentRequest, Mockito.never())
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
        queryPaymentApps();
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1))
                .onShowCalledAndAppsQueriedAndDetailsFinalized();
    }

    @Test
    @Feature({"Payments"})
    public void testCloseTeardownResources() {
        PaymentRequestService service = defaultBuilder().build();
        Mockito.verify(mBrowserPaymentRequest, Mockito.never()).close();
        assertClosed(false);
        service.close();
        Mockito.verify(mBrowserPaymentRequest, Mockito.times(1)).close();
        assertClosed(true);
    }

    @Test
    @Feature({"Payments"})
    public void testOnlyOneServiceCanBeShownGlobally() {
        PaymentRequestService service1 = defaultBuilder().build();
        show(service1);
        assertNoError();
        PaymentRequestService service2 = defaultBuilder().build();
        show(service2);
        assertErrorAndReason(ErrorStrings.ANOTHER_UI_SHOWING, PaymentErrorReason.ALREADY_SHOWING);
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_success() {
        Assert.assertNotNull(defaultBuilder().setOnlySpcMethodWithoutPaymentOptions().build());
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_failedForHavingOptions() {
        PaymentOptions options = new PaymentOptions();
        options.requestShipping = true;
        Assert.assertNull(
                defaultBuilder()
                        .setOnlySpcMethodWithoutPaymentOptions()
                        .setOptions(options)
                        .build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_failedForNullPayeeNameAndOrigin() {
        Assert.assertNull(
                defaultBuilder()
                        .setPayeeName(null)
                        .setPayeeOrigin(null)
                        .setOnlySpcMethodWithoutPaymentOptions()
                        .build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_allowsNullPayeeOrigin() {
        // If a valid payeeName is passed, then payeeOrigin is not needed.
        Assert.assertNotNull(
                defaultBuilder()
                        .setOnlySpcMethodWithoutPaymentOptions()
                        .setPayeeName("Merchant Shop")
                        .setPayeeOrigin(null)
                        .build());
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_failedForEmptyPayeeName() {
        Assert.assertNull(
                defaultBuilder().setPayeeName("").setOnlySpcMethodWithoutPaymentOptions().build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testSpcCanOnlyBeRequestedAlone_failedForHttpPayeeOrigin() {
        org.chromium.url.internal.mojom.Origin payeeOrigin =
                new org.chromium.url.internal.mojom.Origin();
        payeeOrigin.scheme = "http";
        payeeOrigin.host = "www.example.test";
        payeeOrigin.port = 443;
        Assert.assertNull(
                defaultBuilder()
                        .setPayeeOrigin(payeeOrigin)
                        .setOnlySpcMethodWithoutPaymentOptions()
                        .build());
        assertErrorAndReason(
                ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    @Test
    @Feature({"Payments"})
    public void testActivationlessShow() {
        mJniMocker.mock(PaymentRequestWebContentsDataJni.TEST_HOOKS, mWebContentsDataJniMock);
        // The first show() with no user gesture is allowed.
        mIsUserGestureShow = false;
        PaymentRequestService service = defaultBuilder().setOptions(new PaymentOptions()).build();
        show(service);
        assertNoError();
        assertClosed(false);
        service.close();
        assertClosed(true);

        Mockito.verify(mWebContentsDataJniMock, Mockito.times(1))
                .recordActivationlessShow(Mockito.any());
        Mockito.doReturn(true).when(mWebContentsDataJniMock).hadActivationlessShow(Mockito.any());

        // A second show() with no user gesture is not allowed.
        service = defaultBuilder().setOptions(new PaymentOptions()).build();
        show(service);
        assertErrorAndReason(
                ErrorStrings.CANNOT_SHOW_WITHOUT_USER_ACTIVATION,
                PaymentErrorReason.USER_ACTIVATION_REQUIRED);
        assertClosed(true);
        resetErrorMessageAndCloseState();

        // A following show() with a user gesture is allowed.
        mIsUserGestureShow = true;
        service = defaultBuilder().setOptions(new PaymentOptions()).build();
        show(service);
        assertNoError();
        assertClosed(false);
        service.close();
        assertClosed(true);

        Mockito.verify(mWebContentsDataJniMock, Mockito.times(1))
                .recordActivationlessShow(Mockito.any());
    }
}
