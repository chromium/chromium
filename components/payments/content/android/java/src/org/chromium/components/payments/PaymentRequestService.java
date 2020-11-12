// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;

import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.payments.BrowserPaymentRequest.Factory;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentShippingType;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * {@link PaymentRequestService}, {@link MojoPaymentRequestGateKeeper} and
 * ChromePaymentRequestService together make up the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This class provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.ChromePaymentRequestService.
 * TODO(crbug.com/1102522): ChromePaymentRequestService is under refactoring, with the purpose of
 * moving the business logic of ChromePaymentRequestService into PaymentRequestService and
 * eventually moving ChromePaymentRequestService. Note that the callers of the instances of this
 * class need to close them with
 * {@link PaymentRequestService#close()}, after which no usage is allowed.
 */
public class PaymentRequestService
        implements PaymentAppFactoryDelegate, PaymentAppFactoryParams,
                   PaymentRequestUpdateEventListener, PaymentApp.AbortCallback,
                   PaymentApp.InstrumentDetailsCallback, PaymentDetailsConverter.MethodChecker,
                   PaymentResponseHelperInterface.PaymentResponseResultCallback {
    private static final String TAG = "PaymentRequestServ";
    /**
     * Hold the currently showing PaymentRequest. Used to prevent showing more than one
     * PaymentRequest UI per browser process.
     */
    private static PaymentRequestService sShowingPaymentRequest;

    private static PaymentRequestServiceObserverForTest sObserverForTest;
    private static NativeObserverForTest sNativeObserverForTest;
    private static boolean sIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest;
    private final Runnable mOnClosedListener;
    private final WebContents mWebContents;
    private final JourneyLogger mJourneyLogger;
    private final RenderFrameHost mRenderFrameHost;
    private final String mTopLevelOrigin;
    private final String mPaymentRequestOrigin;
    private final Origin mPaymentRequestSecurityOrigin;
    private final String mMerchantName;
    @Nullable
    private final byte[][] mCertificateChain;
    private final boolean mIsOffTheRecord;
    private final PaymentOptions mPaymentOptions;
    private final boolean mRequestShipping;
    private final boolean mRequestPayerName;
    private final boolean mRequestPayerPhone;
    private final boolean mRequestPayerEmail;
    private final Delegate mDelegate;
    private final int mShippingType;
    private final List<PaymentApp> mPendingApps = new ArrayList<>();
    private PaymentRequestSpec mSpec;
    private boolean mHasClosed;
    private boolean mIsFinishedQueryingPaymentApps;
    private boolean mIsCurrentPaymentRequestShowing;
    private boolean mIsShowWaitingForUpdatedDetails;

    /** If not empty, use this error message for rejecting PaymentRequest.show(). */
    private String mRejectShowErrorMessage;

    /** Whether PaymentRequest.show() was invoked with a user gesture. */
    private boolean mIsUserGestureShow;

    // mClient is null only when it has closed.
    @Nullable
    private PaymentRequestClient mClient;

    // mBrowserPaymentRequest is null when it has closed or is uninitiated.
    @Nullable
    private BrowserPaymentRequest mBrowserPaymentRequest;

    /** The helper to create and fill the response to send to the merchant. */
    @Nullable
    private PaymentResponseHelperInterface mPaymentResponseHelper;

    /**
     * A mapping of the payment method names to the corresponding payment method specific data. If
     * STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT is enabled, then the key "basic-card-payment-options"
     * also maps to the following payment options:
     *  - requestPayerEmail
     *  - requestPayerName
     *  - requestPayerPhone
     *  - requestShipping
     */
    private HashMap<String, PaymentMethodData> mQueryForQuota;
    /**
     * True after at least one usable payment app has been found and the setting allows querying
     * this value. This value can be used to respond to hasEnrolledInstrument(). Should be read only
     * after all payment apps have been queried.
     */
    private boolean mHasEnrolledInstrument;
    /** True if any of the requested payment methods are supported. */
    private boolean mCanMakePayment;
    /**
     * Whether there's at least one app that is not an autofill card. Should be read only after all
     * payment apps have been queried.
     */
    private boolean mHasNonAutofillApp;

    private boolean mIsCanMakePaymentResponsePending;
    private boolean mIsHasEnrolledInstrumentResponsePending;
    @Nullable
    private PaymentApp mInvokedPaymentApp;

    /**
     * An observer interface injected when running tests to allow them to observe events.
     * This interface holds events that should be passed back to the native C++ test
     * harness and mirrors the C++ PaymentRequest::ObserverForTest() interface. Its methods
     * should be called in the same places that the C++ PaymentRequest object will call its
     * ObserverForTest.
     */
    public interface NativeObserverForTest {
        void onCanMakePaymentCalled();
        void onCanMakePaymentReturned();
        void onHasEnrolledInstrumentCalled();
        void onHasEnrolledInstrumentReturned();
        void onAppListReady(@Nullable List<EditableOption> paymentApps, PaymentItem total);
        void onNotSupportedError();
        void onConnectionTerminated();
        void onAbortCalled();
        void onCompleteCalled();
        void onMinimalUIReady();
        void onPaymentUiServiceCreated(PaymentUiServiceTestInterface uiService);
        void onClosed();
    }

    /**
     * A delegate to ask questions about the system, that allows tests to inject behaviour without
     * having to modify the entire system. This partially mirrors a similar C++
     * (Content)PaymentRequestDelegate for the C++ implementation, allowing the test harness to
     * override behaviour in both in a similar fashion.
     */
    public interface Delegate {
        /**
         * @return Whether the merchant's WebContents is currently showing an off-the-record tab.
         *         Return true if the tab profile is not accessible from the WebContents.
         */
        boolean isOffTheRecord();

        /**
         * @return A non-null string if there is an invalid SSL certificate on the currently loaded
         *         page.
         */
        String getInvalidSslCertificateErrorMessage();

        /**
         * @return Whether the preferences allow CAN_MAKE_PAYMENT.
         */
        boolean prefsCanMakePayment();

        /**
         * @return If the merchant's WebContents is running inside of a Trusted Web Activity,
         *         returns the package name for Trusted Web Activity. Otherwise returns an empty
         *         string or null.
         */
        @Nullable
        String getTwaPackageName();
    }

    /**
     * A test-only observer for the PaymentRequest service implementation.
     */
    public interface PaymentRequestServiceObserverForTest {
        /**
         * Called when an abort request was denied.
         */
        void onPaymentRequestServiceUnableToAbort();

        /**
         * Called when the controller is notified of billing address change, but does not alter the
         * editor UI.
         */
        void onPaymentRequestServiceBillingAddressChangeProcessed();

        /**
         * Called when the controller is notified of an expiration month change.
         */
        void onPaymentRequestServiceExpirationMonthChange();

        /**
         * Called when a show request failed. This can happen when:
         * <ul>
         *   <li>The merchant requests only unsupported payment methods.</li>
         *   <li>The merchant requests only payment methods that don't have corresponding apps and
         *   are not able to add a credit card from PaymentRequest UI.</li>
         * </ul>
         */
        void onPaymentRequestServiceShowFailed();

        /**
         * Called when the canMakePayment() request has been responded to.
         */
        void onPaymentRequestServiceCanMakePaymentQueryResponded();

        /**
         * Called when the hasEnrolledInstrument() request has been responded to.
         */
        void onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();

        /**
         * Called when the payment response is ready.
         */
        void onPaymentResponseReady();

        /**
         * Called when the browser acknowledges the renderer's complete call, which indicates that
         * the browser UI has closed.
         */
        void onCompleteReplied();

        /**
         * Called when the renderer is closing the mojo connection (e.g. upon show promise
         * rejection).
         */
        void onRendererClosedMojoConnection();
    }

    /**
     * Create an instance of {@link PaymentRequest} that provides the Android PaymentRequest
     * service.
     * @param renderFrameHost The RenderFrameHost of the merchant page.
     * @param isOffTheRecord Whether the merchant page is in a off-the-record (e.g., incognito,
     *         guest mode) Tab.
     * @param delegate The delegate of this class.
     * @param browserPaymentRequestFactory The factory that generates BrowserPaymentRequest.
     * @return The created instance.
     */
    public static PaymentRequest createPaymentRequest(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, Delegate delegate,
            BrowserPaymentRequest.Factory browserPaymentRequestFactory) {
        return new MojoPaymentRequestGateKeeper(
                (client, methodData, details, options, googlePayBridgeEligible, onClosedListener)
                        -> createIfParamsValid(renderFrameHost, isOffTheRecord,
                                browserPaymentRequestFactory, client, methodData, details, options,
                                googlePayBridgeEligible, onClosedListener, delegate));
    }

    /**
     * @return An instance of {@link PaymentRequestService} only if the parameters are deemed
     *         valid; Otherwise, null.
     */
    @Nullable
    private static PaymentRequestService createIfParamsValid(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, BrowserPaymentRequest.Factory browserPaymentRequestFactory,
            @Nullable PaymentRequestClient client, @Nullable PaymentMethodData[] methodData,
            @Nullable PaymentDetails details, @Nullable PaymentOptions options,
            boolean googlePayBridgeEligible, Runnable onClosedListener, Delegate delegate) {
        assert renderFrameHost != null;
        assert browserPaymentRequestFactory != null;
        assert onClosedListener != null;

        if (renderFrameHost.getLastCommittedOrigin() == null
                || renderFrameHost.getLastCommittedURL() == null) {
            abortBeforeInstantiation(/*client=*/null, /*journeyLogger=*/null, ErrorStrings.NO_FRAME,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);
        if (webContents == null || webContents.isDestroyed()) {
            abortBeforeInstantiation(/*client=*/null, /*journeyLogger=*/null,
                    ErrorStrings.NO_WEB_CONTENTS, AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        JourneyLogger journeyLogger = new JourneyLogger(isOffTheRecord, webContents);

        if (client == null) {
            abortBeforeInstantiation(/*client=*/null, journeyLogger, ErrorStrings.INVALID_STATE,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        if (!OriginSecurityChecker.isOriginSecure(webContents.getLastCommittedUrl())) {
            abortBeforeInstantiation(client, journeyLogger, ErrorStrings.NOT_IN_A_SECURE_ORIGIN,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        if (methodData == null) {
            abortBeforeInstantiation(client, journeyLogger,
                    ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        // details has default value, so could never be null, according to payment_request.idl.
        if (details == null) {
            abortBeforeInstantiation(client, journeyLogger, ErrorStrings.INVALID_PAYMENT_DETAILS,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        // options has default value, so could never be null, according to
        // payment_request.idl.
        if (options == null) {
            abortBeforeInstantiation(client, journeyLogger, ErrorStrings.INVALID_PAYMENT_OPTIONS,
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            return null;
        }

        PaymentRequestService instance = new PaymentRequestService(client, renderFrameHost,
                webContents, journeyLogger, options, isOffTheRecord, onClosedListener, delegate);
        boolean valid = instance.initAndValidate(
                browserPaymentRequestFactory, methodData, details, googlePayBridgeEligible);
        if (!valid) {
            instance.close();
            return null;
        }
        instance.startPaymentAppService();
        return instance;
    }

    private void startPaymentAppService() {
        PaymentAppService service = PaymentAppService.getInstance();
        mBrowserPaymentRequest.addPaymentAppFactories(service);
        service.create(/*delegate=*/this);
    }

    /**
     * @return Whether the payment details is pending to be updated due to a promise that was
     *         passed into PaymentRequest.show().
     */
    public boolean isShowWaitingForUpdatedDetails() {
        return mIsShowWaitingForUpdatedDetails;
    }

    /**
     * Sets that the payment details is no longer pending to be updated because the promise that
     * was passed into PaymentRequest.show() has been resolved.
     */
    public void resetWaitingForUpdatedDetails() {
        mIsShowWaitingForUpdatedDetails = false;
    }

    /**
     * Called to open a new PaymentHandler UI on the showing PaymentRequest.
     * @param url The url of the payment app to be displayed in the UI.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *         successful; null if failed.
     */
    @Nullable
    public static WebContents openPaymentHandlerWindow(GURL url) {
        if (sShowingPaymentRequest == null) return null;
        return sShowingPaymentRequest.mBrowserPaymentRequest.openPaymentHandlerWindow(url);
    }

    /**
     * Disconnects from the PaymentRequestClient with a debug message.
     * @param debugMessage The debug message shown for web developers.
     * @param reason The reason of the disconnection defined in {@link PaymentErrorReason}.
     */
    public void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mClient != null) {
            mClient.onError(reason, debugMessage);
        }
        close();
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /** Abort the request, used before this class's instantiation. */
    private static void abortBeforeInstantiation(@Nullable PaymentRequestClient client,
            @Nullable JourneyLogger journeyLogger, String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (client != null) client.onError(reason, debugMessage);
        if (journeyLogger != null) journeyLogger.setAborted(reason);
        if (sNativeObserverForTest != null) sNativeObserverForTest.onConnectionTerminated();
    }

    private PaymentRequestService(PaymentRequestClient client, RenderFrameHost renderFrameHost,
            WebContents webContents, JourneyLogger journeyLogger, PaymentOptions options,
            boolean isOffTheRecord, Runnable onClosedListener, Delegate delegate) {
        assert client != null;
        assert renderFrameHost != null;
        assert webContents != null;
        assert journeyLogger != null;
        assert options != null;
        assert onClosedListener != null;
        assert delegate != null;

        mRenderFrameHost = renderFrameHost;
        mPaymentRequestSecurityOrigin = mRenderFrameHost.getLastCommittedOrigin();
        mWebContents = webContents;

        // TODO(crbug.com/992593): replace UrlFormatter with GURL operations.
        mPaymentRequestOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(mRenderFrameHost.getLastCommittedURL());
        mTopLevelOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(mWebContents.getLastCommittedUrl());

        mPaymentOptions = options;
        mRequestShipping = mPaymentOptions.requestShipping;
        mRequestPayerName = mPaymentOptions.requestPayerName;
        mRequestPayerPhone = mPaymentOptions.requestPayerPhone;
        mRequestPayerEmail = mPaymentOptions.requestPayerEmail;
        mShippingType = mPaymentOptions.shippingType;

        mMerchantName = mWebContents.getTitle();
        mCertificateChain = CertificateChainHelper.getCertificateChain(mWebContents);
        mIsOffTheRecord = isOffTheRecord;
        mClient = client;
        mJourneyLogger = journeyLogger;
        mOnClosedListener = onClosedListener;
        mDelegate = delegate;
        mHasClosed = false;
    }

    /**
     * Set a native-side observer for PaymentRequest implementations. This observer should be set
     * before PaymentRequest implementations are instantiated.
     * @param nativeObserverForTest The native-side observer.
     */
    @VisibleForTesting
    public static void setNativeObserverForTest(NativeObserverForTest nativeObserverForTest) {
        sNativeObserverForTest = nativeObserverForTest;
    }

    /** @return Get the native=side observer, for testing purpose only. */
    @Nullable
    public static NativeObserverForTest getNativeObserverForTest() {
        return sNativeObserverForTest;
    }

    private boolean initAndValidate(Factory factory, PaymentMethodData[] rawMethodData,
            PaymentDetails details, boolean googlePayBridgeEligible) {
        mBrowserPaymentRequest = factory.createBrowserPaymentRequest(this);
        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.INITIATED);

        if (!UrlUtil.isOriginAllowedToUseWebPaymentApis(mWebContents.getLastCommittedUrl())) {
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN);
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.PROHIBITED_ORIGIN,
                    PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
            return false;
        }

        mJourneyLogger.setRequestedInformation(
                mRequestShipping, mRequestPayerEmail, mRequestPayerPhone, mRequestPayerName);

        String rejectShowErrorMessage = mDelegate.getInvalidSslCertificateErrorMessage();
        if (!TextUtils.isEmpty(rejectShowErrorMessage)) {
            Log.d(TAG, rejectShowErrorMessage);
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(rejectShowErrorMessage,
                    PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
            return false;
        }

        mBrowserPaymentRequest.onWhetherGooglePayBridgeEligible(
                googlePayBridgeEligible, mWebContents, rawMethodData);
        @Nullable
        Map<String, PaymentMethodData> methodData = getValidatedMethodData(rawMethodData);
        mBrowserPaymentRequest.modifyMethodData(methodData);
        if (methodData == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA, PaymentErrorReason.USER_CANCEL);
            return false;
        }
        methodData = Collections.unmodifiableMap(methodData);

        mQueryForQuota = new HashMap<>(methodData);
        mBrowserPaymentRequest.onQueryForQuotaCreated(mQueryForQuota);

        if (!PaymentValidator.validatePaymentDetails(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
            return false;
        }

        if (mBrowserPaymentRequest.disconnectIfExtraValidationFails(
                    mWebContents, methodData, details, mPaymentOptions)) {
            return false;
        }

        PaymentRequestSpec spec = new PaymentRequestSpec(mPaymentOptions, details,
                methodData.values(), LocaleUtils.getDefaultLocaleString());
        if (spec.getRawTotal() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.TOTAL_REQUIRED, PaymentErrorReason.USER_CANCEL);
            return false;
        }
        mSpec = spec;
        mBrowserPaymentRequest.onSpecValidated(mSpec);
        return true;
    }

    // Implements PaymentResponseHelper.PaymentResponseResultCallback:
    @Override
    public void onPaymentResponseReady(PaymentResponse response) {
        if (!mBrowserPaymentRequest.patchPaymentResponseIfNeeded(response)) {
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.PAYMENT_APP_INVALID_RESPONSE, PaymentErrorReason.NOT_SUPPORTED);
            // Intentionally do not early-return.
        }
        if (mClient != null) {
            mClient.onPaymentResponse(response);
        }
        mPaymentResponseHelper = null;
        if (sObserverForTest != null) {
            sObserverForTest.onPaymentResponseReady();
        }
    }

    /**
     * Invokes the given payment app.
     * @param paymentApp The payment app to be invoked.
     * @param paymentResponseHelper The helper to create and fill the response to send to the
     *         merchant. The helper should have this instance as the delegate {@link
     *         PaymentResponseHelperInterface.PaymentResponseRequesterDelegate}.
     */
    public void invokePaymentApp(
            PaymentApp paymentApp, PaymentResponseHelperInterface paymentResponseHelper) {
        mPaymentResponseHelper = paymentResponseHelper;
        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.PAYMENT_HANDLER_INVOKED);
        // Create maps that are subsets of mMethodData and mModifiers, that contain the payment
        // methods supported by the selected payment app. If the intersection of method data
        // contains more than one payment method, the payment app is at liberty to choose (or have
        // the user choose) one of the methods.
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        Map<String, PaymentDetailsModifier> modifiers = new HashMap<>();
        for (String paymentMethodName : paymentApp.getInstrumentMethodNames()) {
            if (mSpec.getMethodData().containsKey(paymentMethodName)) {
                methodData.put(paymentMethodName, mSpec.getMethodData().get(paymentMethodName));
            }
            if (mSpec.getModifiers().containsKey(paymentMethodName)) {
                modifiers.put(paymentMethodName, mSpec.getModifiers().get(paymentMethodName));
            }
        }

        // Create payment options for the invoked payment app.
        PaymentOptions paymentOptions = new PaymentOptions();
        paymentOptions.requestShipping = mRequestShipping && paymentApp.handlesShippingAddress();
        paymentOptions.requestPayerName = mRequestPayerName && paymentApp.handlesPayerName();
        paymentOptions.requestPayerPhone = mRequestPayerPhone && paymentApp.handlesPayerPhone();
        paymentOptions.requestPayerEmail = mRequestPayerEmail && paymentApp.handlesPayerEmail();
        paymentOptions.shippingType = mRequestShipping && paymentApp.handlesShippingAddress()
                ? mShippingType
                : PaymentShippingType.SHIPPING;

        // Redact shipping options if the selected app cannot handle shipping.
        List<PaymentShippingOption> redactedShippingOptions = paymentApp.handlesShippingAddress()
                ? mSpec.getRawShippingOptions()
                : Collections.unmodifiableList(new ArrayList<>());
        paymentApp.invokePaymentApp(mSpec.getId(), mMerchantName, mTopLevelOrigin,
                mPaymentRequestOrigin, mCertificateChain, Collections.unmodifiableMap(methodData),
                mSpec.getRawTotal(), mSpec.getRawLineItems(),
                Collections.unmodifiableMap(modifiers), paymentOptions, redactedShippingOptions,
                /*callback=*/this);
        mInvokedPaymentApp = paymentApp;
        mJourneyLogger.setEventOccurred(Event.PAY_CLICKED);
        boolean isAutofillCard = paymentApp.isAutofillInstrument();
        // Record what type of app was selected when "Pay" was clicked.
        boolean isGooglePaymentApp = false;
        for (String paymentMethodName : paymentApp.getInstrumentMethodNames()) {
            if (paymentMethodName.equals(MethodStrings.ANDROID_PAY)
                    || paymentMethodName.equals(MethodStrings.GOOGLE_PAY)) {
                isGooglePaymentApp = true;
                break;
            }
        }
        if (isAutofillCard) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_CREDIT_CARD);
        } else if (isGooglePaymentApp) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_GOOGLE);
        } else {
            mJourneyLogger.setEventOccurred(Event.SELECTED_OTHER);
        }
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory /* Unused */) {
        if (mBrowserPaymentRequest == null) return;
        mIsFinishedQueryingPaymentApps = true;

        if (disconnectIfNoPaymentMethodsSupported(mBrowserPaymentRequest.hasAvailableApps())) {
            return;
        }

        // Always return false when can make payment is disabled.
        mHasEnrolledInstrument &= mDelegate.prefsCanMakePayment();

        if (mIsCanMakePaymentResponsePending) {
            respondCanMakePaymentQuery();
        }

        if (mIsHasEnrolledInstrumentResponsePending) {
            respondHasEnrolledInstrumentQuery();
        }

        mBrowserPaymentRequest.notifyPaymentUiOfPendingApps(mPendingApps);
        mPendingApps.clear();
        if (isCurrentPaymentRequestShowing()
                && !mBrowserPaymentRequest.showAppSelector(mIsShowWaitingForUpdatedDetails)) {
            return;
        }

        mBrowserPaymentRequest.triggerPaymentAppUiSkipIfApplicable();
    }

    /**
     * If no payment methods are supported, disconnect from the client and return true.
     * @param hasAvailableApps Whether any payment app is available.
     * @return Whether client has been disconnected.
     */
    private boolean disconnectIfNoPaymentMethodsSupported(boolean hasAvailableApps) {
        if (!mIsFinishedQueryingPaymentApps || !isCurrentPaymentRequestShowing()) return false;
        if (!mCanMakePayment || (mPendingApps.isEmpty() && !hasAvailableApps)) {
            // All factories have responded, but none of them have apps. It's possible to add credit
            // cards, but the merchant does not support them either. The payment request must be
            // rejected.
            mJourneyLogger.setNotShown(mCanMakePayment
                            ? NotShownReason.NO_MATCHING_PAYMENT_METHOD
                            : NotShownReason.NO_SUPPORTED_PAYMENT_METHOD);
            if (mDelegate.isOffTheRecord()) {
                // If the user is in the OffTheRecord mode, hide the absence of their payment
                // methods from the merchant site.
                disconnectFromClientWithDebugMessage(
                        ErrorStrings.USER_CANCELLED, PaymentErrorReason.USER_CANCEL);
            } else {
                if (sNativeObserverForTest != null) {
                    sNativeObserverForTest.onNotSupportedError();
                }

                if (TextUtils.isEmpty(mRejectShowErrorMessage) && !isInTwa()
                        && mSpec.getMethodData().get(MethodStrings.GOOGLE_PLAY_BILLING) != null) {
                    mRejectShowErrorMessage = ErrorStrings.APP_STORE_METHOD_ONLY_SUPPORTED_IN_TWA;
                }
                disconnectFromClientWithDebugMessage(
                        ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet())
                                + (TextUtils.isEmpty(mRejectShowErrorMessage)
                                                ? ""
                                                : " " + mRejectShowErrorMessage),
                        PaymentErrorReason.NOT_SUPPORTED);
            }
            if (sObserverForTest != null) {
                sObserverForTest.onPaymentRequestServiceShowFailed();
            }
            return true;
        }
        return disconnectForStrictShow(mIsUserGestureShow);
    }

    /**
     * If strict show() conditions are not satisfied, disconnect from client and return true.
     * @param isUserGestureShow Whether the PaymentRequest.show() is triggered by user gesture.
     * @return Whether client has been disconnected.
     */
    private boolean disconnectForStrictShow(boolean isUserGestureShow) {
        if (!isUserGestureShow || !mSpec.getMethodData().containsKey(MethodStrings.BASIC_CARD)
                || mHasEnrolledInstrument || mHasNonAutofillApp
                || !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            return false;
        }

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceShowFailed();
        }
        mRejectShowErrorMessage = ErrorStrings.STRICT_BASIC_CARD_SHOW_REJECT;
        disconnectFromClientWithDebugMessage(
                ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet()) + " "
                        + mRejectShowErrorMessage,
                PaymentErrorReason.NOT_SUPPORTED);

        return true;
    }

    private boolean isInTwa() {
        return !TextUtils.isEmpty(mDelegate.getTwaPackageName());
    }

    /** @return Whether PaymentRequest.show() was invoked with a user gesture. */
    public boolean isUserGestureShow() {
        return mIsUserGestureShow;
    }

    /**
     * Records that PaymentRequest.show() was invoked with a user gesture.
     * @param userGestureShow Whether it is invoked with a user gesture.
     */
    public void setUserGestureShow(boolean userGestureShow) {
        mIsUserGestureShow = userGestureShow;
    }

    /** @return Whether the current payment request service has called show(). */
    public boolean isCurrentPaymentRequestShowing() {
        return mIsCurrentPaymentRequestShowing;
    }

    /**
     * Records whether the current payment request service has called show().
     * @param isShowing Whether show() has been called.
     */
    public void setCurrentPaymentRequestShowing(boolean isShowing) {
        mIsCurrentPaymentRequestShowing = isShowing;
    }

    /**
     * @return Whether all payment apps have been queried of canMakePayment() and
     *         hasEnrolledInstrument().
     */
    public boolean isFinishedQueryingPaymentApps() {
        return mIsFinishedQueryingPaymentApps;
    }

    @VisibleForTesting
    public static void setIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest() {
        sIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest = true;
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public PaymentAppFactoryParams getParams() {
        return this;
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onPaymentAppCreated(paymentApp);
        mHasEnrolledInstrument |= paymentApp.canMakePayment();
        mHasNonAutofillApp |= !paymentApp.isAutofillInstrument();

        if (paymentApp.isAutofillInstrument()) {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_BASIC_CARD);
        } else if (paymentApp.getInstrumentMethodNames().contains(MethodStrings.GOOGLE_PAY)
                || paymentApp.getInstrumentMethodNames().contains(MethodStrings.ANDROID_PAY)) {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_GOOGLE);
        } else {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_OTHER);
        }

        mPendingApps.add(paymentApp);
    }

    /** Responds to the CanMakePayment query from the merchant page. */
    public void respondCanMakePaymentQuery() {
        if (mClient == null) return;

        mIsCanMakePaymentResponsePending = false;

        boolean response = mCanMakePayment && mDelegate.prefsCanMakePayment();
        mClient.onCanMakePayment(response ? CanMakePaymentQueryResult.CAN_MAKE_PAYMENT
                                          : CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);

        mJourneyLogger.setCanMakePaymentValue(response || mIsOffTheRecord);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceCanMakePaymentQueryResponded();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onCanMakePaymentReturned();
        }
    }

    /** Responds to the HasEnrolledInstrument query from the merchant page. */
    public void respondHasEnrolledInstrumentQuery() {
        if (mClient == null) return;
        boolean response = mHasEnrolledInstrument;
        mIsHasEnrolledInstrumentResponsePending = false;

        int result;
        if (CanMakePaymentQuery.canQuery(
                    mWebContents, mTopLevelOrigin, mPaymentRequestOrigin, mQueryForQuota)) {
            result = response ? HasEnrolledInstrumentQueryResult.HAS_ENROLLED_INSTRUMENT
                              : HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT;
        } else if (shouldEnforceHasEnrolledInstrumentQueryQuota()) {
            result = HasEnrolledInstrumentQueryResult.QUERY_QUOTA_EXCEEDED;
        } else {
            result = response ? HasEnrolledInstrumentQueryResult.WARNING_HAS_ENROLLED_INSTRUMENT
                              : HasEnrolledInstrumentQueryResult.WARNING_HAS_NO_ENROLLED_INSTRUMENT;
        }
        mClient.onHasEnrolledInstrument(result);

        mJourneyLogger.setHasEnrolledInstrumentValue(response || mIsOffTheRecord);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onHasEnrolledInstrumentReturned();
        }
    }

    /**
     * @return Whether hasEnrolledInstrument() query quota should be enforced. By default, the quota
     *         is enforced only on https:// scheme origins. However, the tests also enable the quota
     *         on localhost and file:// scheme origins to verify its behavior.
     */
    private boolean shouldEnforceHasEnrolledInstrumentQueryQuota() {
        // If |mWebContents| is destroyed, don't bother checking the localhost or file:// scheme
        // exemption. It doesn't really matter anyways.
        return mWebContents.isDestroyed()
                || !UrlUtil.isLocalDevelopmentUrl(mWebContents.getLastCommittedUrl())
                || sIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest;
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onCanMakePaymentCalculated(boolean canMakePayment) {
        mCanMakePayment = canMakePayment;
        if (!mIsCanMakePaymentResponsePending) return;
        // canMakePayment doesn't need to wait for all apps to be queried because it only needs to
        // test the existence of a payment handler.
        respondCanMakePaymentQuery();
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onPaymentAppCreationError(String errorMessage) {
        if (TextUtils.isEmpty(mRejectShowErrorMessage)) {
            mRejectShowErrorMessage = errorMessage;
        }
    }

    /** @return Whether the created payment apps includes any autofill payment app. */
    public boolean getHasNonAutofillApp() {
        return mHasNonAutofillApp;
    }

    /**
     * @param methodDataList A list of PaymentMethodData.
     * @return The validated method data, a mapping of method names to its PaymentMethodData(s);
     *         when the given method data is invalid, returns null.
     */
    @Nullable
    private static Map<String, PaymentMethodData> getValidatedMethodData(
            PaymentMethodData[] methodDataList) {
        // Payment methodData are required.
        assert methodDataList != null;
        if (methodDataList.length == 0) return null;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (PaymentMethodData methodData : methodDataList) {
            String methodName = methodData.supportedMethod;
            if (TextUtils.isEmpty(methodName)) return null;
            result.put(methodName, methodData);
        }
        return result;
    }

    /**
     * The component part of the {@link PaymentRequest#show} implementation. Check {@link
     * PaymentRequest#show} for the parameters' specification.
     */
    /* package */ void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        if (mBrowserPaymentRequest == null) return;
        if (mBrowserPaymentRequest.isShowingUi()) {
            // Can be triggered only by a compromised renderer. In normal operation, calling show()
            // twice on the same instance of PaymentRequest in JavaScript is rejected at the
            // renderer level.
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.CANNOT_SHOW_TWICE, PaymentErrorReason.USER_CANCEL);
            return;
        }
        if (sShowingPaymentRequest != null) {
            // The renderer can create multiple instances of PaymentRequest and call show() on each
            // one. Only the first one will be shown. This also prevents multiple tabs and windows
            // from showing PaymentRequest UI at the same time.
            mJourneyLogger.setNotShown(NotShownReason.CONCURRENT_REQUESTS);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.ANOTHER_UI_SHOWING, PaymentErrorReason.ALREADY_SHOWING);
            if (sObserverForTest != null) {
                sObserverForTest.onPaymentRequestServiceShowFailed();
            }
            return;
        }
        sShowingPaymentRequest = this;
        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.SHOW_CALLED);
        mIsCurrentPaymentRequestShowing = true;
        mIsUserGestureShow = isUserGesture;
        mIsShowWaitingForUpdatedDetails = waitForUpdatedDetails;

        mJourneyLogger.setTriggerTime();
        if (disconnectIfNoPaymentMethodsSupported(mBrowserPaymentRequest.hasAvailableApps())) {
            return;
        }
        if (isFinishedQueryingPaymentApps()
                && !mBrowserPaymentRequest.showAppSelector(mIsShowWaitingForUpdatedDetails)) {
            return;
        }

        mBrowserPaymentRequest.triggerPaymentAppUiSkipIfApplicable();
    }

    // Implements PaymentDetailsConverter.MethodChecker:
    @Override
    public boolean isInvokedInstrumentValidForPaymentMethodIdentifier(
            String methodName, PaymentApp invokedPaymentApp) {
        return invokedPaymentApp != null
                && invokedPaymentApp.isValidForPaymentMethodData(methodName, null);
    }

    private void continueShow(PaymentDetails details) {
        assert mIsShowWaitingForUpdatedDetails;
        // mSpec.updateWith() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (!PaymentValidator.validatePaymentDetails(details)
                || !mBrowserPaymentRequest.parseAndValidateDetailsFurtherIfNeeded(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
            return;
        }

        if (!TextUtils.isEmpty(details.error)) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_STATE, PaymentErrorReason.USER_CANCEL);
            return;
        }

        mSpec.updateWith(details);

        mIsShowWaitingForUpdatedDetails = false;
        mBrowserPaymentRequest.continueShow();
    }

    /**
     * The component part of the {@link PaymentRequest#updateWith} implementation.
     * @param details The details that the merchant provides to update the payment request.
     */
    /* package */ void updateWith(PaymentDetails details) {
        if (mBrowserPaymentRequest == null) return;
        if (mIsShowWaitingForUpdatedDetails) {
            // Under this condition, updateWith() is called in response to the resolution of
            // show()'s PaymentDetailsUpdate promise.
            continueShow(details);
            return;
        }

        if (!mIsCurrentPaymentRequestShowing) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW, PaymentErrorReason.USER_CANCEL);
            return;
        }

        if (!PaymentOptionsUtils.requestAnyInformation(mPaymentOptions)
                && (mInvokedPaymentApp == null
                        || !mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate())) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_STATE, PaymentErrorReason.USER_CANCEL);
            return;
        }

        if (!PaymentValidator.validatePaymentDetails(details)
                || !mBrowserPaymentRequest.parseAndValidateDetailsFurtherIfNeeded(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS, PaymentErrorReason.USER_CANCEL);
            return;
        }
        mSpec.updateWith(details);

        boolean hasNotifiedInvokedPaymentApp =
                mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate();
        if (hasNotifiedInvokedPaymentApp) {
            // After a payment app has been invoked, all of the merchant's calls to update the price
            // via updateWith() should be forwarded to the invoked app, so it can reflect the
            // updated price in its UI.
            mInvokedPaymentApp.updateWith(
                    PaymentDetailsConverter.convertToPaymentRequestDetailsUpdate(details,
                            /*methodChecker=*/this, mInvokedPaymentApp));
        }
        mBrowserPaymentRequest.onPaymentDetailsUpdated(
                mSpec.getPaymentDetails(), hasNotifiedInvokedPaymentApp);
    }

    /**
     * The component part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     */
    /* package */ void onPaymentDetailsNotUpdated() {
        if (mBrowserPaymentRequest == null) return;
        if (!mIsCurrentPaymentRequestShowing) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW, PaymentErrorReason.USER_CANCEL);
            return;
        }
        mSpec.recomputeSpecForDetails();
        if (mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            mInvokedPaymentApp.onPaymentDetailsNotUpdated();
            return;
        }
        mBrowserPaymentRequest.onPaymentDetailsNotUpdated(mSpec.selectedShippingOptionError());
    }

    /** The component part of the {@link PaymentRequest#abort} implementation. */
    /* package */ void abort() {
        if (mInvokedPaymentApp != null) {
            mInvokedPaymentApp.abortPaymentApp(/*callback=*/this);
            return;
        }
        onInstrumentAbortResult(true);
    }

    /** The component part of the {@link PaymentRequest#complete} implementation. */
    /* package */ void complete(int result) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.complete(result);
    }

    /**
     * The component part of the {@link PaymentRequest#retry} implementation. Check {@link
     * PaymentRequest#retry} for the parameters' specification.
     */
    /* package */ void retry(PaymentValidationErrors errors) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.retry(errors);
    }

    /** The component part of the {@link PaymentRequest#canMakePayment} implementation. */
    /* package */ void canMakePayment() {
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onCanMakePaymentCalled();
        }

        if (mIsFinishedQueryingPaymentApps) {
            respondCanMakePaymentQuery();
        } else {
            mIsCanMakePaymentResponsePending = true;
        }
    }

    /**
     * The component part of the {@link PaymentRequest#hasEnrolledInstrument} implementation.
     */
    /* package */ void hasEnrolledInstrument() {
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onHasEnrolledInstrumentCalled();
        }

        if (mIsFinishedQueryingPaymentApps) {
            respondHasEnrolledInstrumentQuery();
        } else {
            mIsHasEnrolledInstrumentResponsePending = true;
        }
    }

    /**
     * Implement {@link PaymentRequest#close}. This should be called by the renderer only. The
     * closing triggered by other classes should call {@link #close} instead. The caller should
     * stop referencing this class after calling this method.
     */
    /* package */ void closeByRenderer() {
        mJourneyLogger.setAborted(AbortReason.MOJO_RENDERER_CLOSING);
        close();
        if (sObserverForTest != null) {
            sObserverForTest.onRendererClosedMojoConnection();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Called when the mojo connection with the renderer PaymentRequest has an error.  The caller
     * should stop referencing this class after calling this method.
     * @param e The mojo exception.
     */
    /* package */ void onConnectionError(MojoException e) {
        mJourneyLogger.setAborted(AbortReason.MOJO_CONNECTION_ERROR);
        close();
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Abort the request because the (untrusted) renderer passes invalid data.
     * @param debugMessage The debug message to be sent to the renderer.
     */
    /* package */ void abortForInvalidDataFromRenderer(String debugMessage) {
        mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    /**
     * Close this instance and release all of the retained resources. The external callers of this
     * method should stop referencing this instance upon calling. This method can be called within
     * itself without causing infinite loops.
     */
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        mIsCurrentPaymentRequestShowing = false;
        sShowingPaymentRequest = null;

        if (mBrowserPaymentRequest != null) {
            mBrowserPaymentRequest.close();
            mBrowserPaymentRequest = null;
        }

        // mClient can be null only when this method is called from
        // PaymentRequestService#create().
        if (mClient != null) {
            mClient.close();
            mClient = null;
        }

        mOnClosedListener.run();

        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onClosed();
        }
    }

    /** @return An observer for the payment request service, if any; otherwise, null. */
    @Nullable
    public static PaymentRequestServiceObserverForTest getObserverForTest() {
        return sObserverForTest;
    }

    /** Set an observer for the payment request service, cannot be null. */
    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        assert observerForTest != null;
        sObserverForTest = observerForTest;
    }

    /** Invokes {@link PaymentRequestClient.onShippingAddressChange}. */
    public void onShippingAddressChange(PaymentAddress address) {
        if (mClient != null) {
            redactShippingAddress(address);
            mClient.onShippingAddressChange(address);
        }
    }

    /** Invokes {@link PaymentRequestClient.onShippingOptionChange}. */
    public void onShippingOptionChange(String shippingOptionId) {
        if (mClient != null) mClient.onShippingOptionChange(shippingOptionId);
    }

    /** Invokes {@link PaymentRequestClient.onPayerDetailChange}. */
    public void onPayerDetailChange(PayerDetail detail) {
        if (mClient != null) mClient.onPayerDetailChange(detail);
    }

    /** Invokes {@link PaymentRequestClient.onError}. */
    public void onError(int error, String errorMessage) {
        if (mClient != null) mClient.onError(error, errorMessage);
    }

    /** Invokes {@link PaymentRequestClient.onComplete}. */
    public void onComplete() {
        if (mClient != null) mClient.onComplete();
    }

    /** Invokes {@link PaymentRequestClient.warnNoFavicon}. */
    public void warnNoFavicon() {
        if (mClient != null) mClient.warnNoFavicon();
    }

    /**
     * @return The logger of the user journey of the Android PaymentRequest service, cannot be
     *         null.
     */
    public JourneyLogger getJourneyLogger() {
        return mJourneyLogger;
    }

    /** @return Whether the WebContents is currently showing an off-the-record tab. */
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    /**
     * Redact shipping address before exposing it in ShippingAddressChangeEvent.
     * https://w3c.github.io/payment-request/#shipping-address-changed-algorithm
     * @param shippingAddress The shipping address to redact in place.
     */
    private static void redactShippingAddress(PaymentAddress shippingAddress) {
        if (PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                    PaymentFeatureList.WEB_PAYMENTS_REDACT_SHIPPING_ADDRESS)) {
            shippingAddress.organization = "";
            shippingAddress.phone = "";
            shippingAddress.recipient = "";
            shippingAddress.addressLine = new String[0];
        }
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean hasClosed() {
        return mHasClosed;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentMethodData> getMethodData() {
        // GetMethodData should not get called after PR is closed.
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getMethodData();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getId() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getId();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getTopLevelOrigin() {
        return mTopLevelOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getPaymentRequestOrigin() {
        return mPaymentRequestOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Origin getPaymentRequestSecurityOrigin() {
        return mPaymentRequestSecurityOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    @Nullable
    public byte[][] getCertificateChain() {
        return mCertificateChain;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentDetailsModifier> getUnmodifiableModifiers() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return Collections.unmodifiableMap(mSpec.getModifiers());
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentItem getRawTotal() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getRawTotal();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean getMayCrawl() {
        return !mBrowserPaymentRequest.isPaymentSheetBasedPaymentAppSupported()
                || PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.WEB_PAYMENTS_ALWAYS_ALLOW_JUST_IN_TIME_PAYMENT_APP);
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentRequestUpdateEventListener getPaymentRequestUpdateEventListener() {
        return this;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentOptions getPaymentOptions() {
        return mPaymentOptions;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentRequestSpec getSpec() {
        return mSpec;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    @Nullable
    public String getTwaPackageName() {
        return mDelegate.getTwaPackageName();
    }

    /** @return The invoked payment app, can be null. */
    @Nullable
    public PaymentApp getInvokedPaymentApp() {
        return mInvokedPaymentApp;
    }

    // Implements PaymentRequestUpdateEventListener:
    @Override
    public boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails) {
        if (TextUtils.isEmpty(methodName) || stringifiedDetails == null
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || mClient == null) {
            return false;
        }
        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
        return true;
    }

    // Implements PaymentRequestUpdateEventListener:
    @Override
    public boolean changeShippingOptionFromInvokedApp(String shippingOptionId) {
        if (TextUtils.isEmpty(shippingOptionId) || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || !mRequestShipping
                || mSpec.getRawShippingOptions() == null || mClient == null) {
            return false;
        }

        boolean isValidId = false;
        for (PaymentShippingOption option : mSpec.getRawShippingOptions()) {
            if (shippingOptionId.equals(option.id)) {
                isValidId = true;
                break;
            }
        }
        if (!isValidId) return false;

        mClient.onShippingOptionChange(shippingOptionId);
        return true;
    }

    // Implements PaymentRequestUpdateEventListener:
    @Override
    public boolean changeShippingAddressFromInvokedApp(PaymentAddress shippingAddress) {
        if (shippingAddress == null || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || !mRequestShipping
                || mClient == null) {
            return false;
        }

        onShippingAddressChange(shippingAddress);
        return true;
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    @Override
    public void onInstrumentDetailsLoadingWithoutUI() {
        if (mPaymentResponseHelper == null || mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onInstrumentDetailsLoading();
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    @Override
    public void onInstrumentDetailsReady(
            String methodName, String stringifiedDetails, PayerData payerData) {
        assert methodName != null;
        assert stringifiedDetails != null;
        if (mPaymentResponseHelper == null || mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onInstrumentDetailsReady();
        mJourneyLogger.setEventOccurred(Event.RECEIVED_INSTRUMENT_DETAILS);
        mPaymentResponseHelper.generatePaymentResponse(
                methodName, stringifiedDetails, payerData, /*resultCallback=*/this);
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    @Override
    public void onInstrumentAbortResult(boolean abortSucceeded) {
        if (mClient != null) {
            mClient.onAbort(abortSucceeded);
        }
        if (abortSucceeded) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_MERCHANT);
            close();
        } else {
            if (sObserverForTest != null) {
                sObserverForTest.onPaymentRequestServiceUnableToAbort();
            }
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onAbortCalled();
        }
    }

    // Implements PaymentApp.AbortCallback:
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
        mInvokedPaymentApp = null;
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onInstrumentDetailsError(errorMessage);
    }
}
