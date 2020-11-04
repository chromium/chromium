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
import org.chromium.components.payments.PaymentApp.InstrumentDetailsCallback;
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
public class PaymentRequestService {
    private static final String TAG = "PaymentRequestServ";
    private static PaymentRequestServiceObserverForTest sObserverForTest;
    private static NativeObserverForTest sNativeObserverForTest;
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
    @Nullable
    private final PaymentOptions mPaymentOptions;
    private final boolean mRequestShipping;
    private final boolean mRequestPayerName;
    private final boolean mRequestPayerPhone;
    private final boolean mRequestPayerEmail;
    private final Delegate mDelegate;
    private final int mShippingType;
    private PaymentRequestSpec mSpec;
    private boolean mHasClosed;

    // mClient is null only when it has closed.
    private PaymentRequestClient mClient;

    // mBrowserPaymentRequest is null when it has closed or is uninitiated.
    private BrowserPaymentRequest mBrowserPaymentRequest;
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
                        -> PaymentRequestService.createIfParamsValid(renderFrameHost,
                                isOffTheRecord, browserPaymentRequestFactory, client, methodData,
                                details, options, googlePayBridgeEligible, onClosedListener,
                                delegate));
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
        service.create(/*delegate=*/mBrowserPaymentRequest.getPaymentAppFactoryDelegate());
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
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
                    ErrorStrings.PROHIBITED_ORIGIN,
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
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(rejectShowErrorMessage,
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
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA, PaymentErrorReason.USER_CANCEL);
            return false;
        }
        methodData = Collections.unmodifiableMap(methodData);

        mQueryForQuota = new HashMap<>(methodData);
        mBrowserPaymentRequest.onQueryForQuotaCreated(mQueryForQuota);

        if (!PaymentValidator.validatePaymentDetails(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
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
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
                    ErrorStrings.TOTAL_REQUIRED, PaymentErrorReason.USER_CANCEL);
            return false;
        }
        mSpec = spec;
        mBrowserPaymentRequest.onSpecValidated(mSpec);
        return true;
    }

    /**
     * Invokes the given payment app.
     * @param paymentApp The payment app to be invoked.
     * @param callback The callback of the invocation.
     */
    public void invokePaymentApp(PaymentApp paymentApp, InstrumentDetailsCallback callback) {
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
                callback);
        mJourneyLogger.setEventOccurred(Event.PAY_CLICKED);
        boolean isAutofillCard = paymentApp.isAutofillInstrument();
        // Record what type of app was selected when "Pay" was clicked.
        boolean isGooglePaymentApp = false;
        for (String paymentMethodName : paymentApp.getInstrumentMethodNames()) {
            if (paymentMethodName.equals(MethodStrings.ANDROID_PAY)
                    || paymentMethodName.equals(MethodStrings.GOOGLE_PAY)) {
                isGooglePaymentApp = true;
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

    /**
     * Called when a payment app is created.
     * @param paymentApp The created payment app.
     * @param pendingApps The list of created apps increasing until onDoneCreatingPaymentApp().
     */
    public void onPaymentAppCreated(PaymentApp paymentApp, List<PaymentApp> pendingApps) {
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

        pendingApps.add(paymentApp);
    }

    /** @return Whether the response of CanMakePayment is pending. */
    public boolean isCanMakePaymentResponsePending() {
        return mIsCanMakePaymentResponsePending;
    }

    /** Sets pending for the response of CanMakePayment. */
    public void setCanMakePaymentResponsePending(boolean isPending) {
        mIsCanMakePaymentResponsePending = isPending;
    }

    /** @return Whether the response of HasEnrolledInstrument is pending. */
    public boolean isHasEnrolledInstrumentResponsePending() {
        return mIsHasEnrolledInstrumentResponsePending;
    }

    /** Sets pending for HasEnrolledInstrument. */
    public void setIsHasEnrolledInstrumentResponsePending(boolean isPending) {
        mIsHasEnrolledInstrumentResponsePending = isPending;
    }

    /** Responds to the CanMakePayment query from the merchant page. */
    public void respondCanMakePaymentQuery() {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

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
        boolean response = mHasEnrolledInstrument;
        mIsHasEnrolledInstrumentResponsePending = false;

        if (CanMakePaymentQuery.canQuery(
                    mWebContents, mTopLevelOrigin, mPaymentRequestOrigin, mQueryForQuota)) {
            onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT);
        } else if (mBrowserPaymentRequest.shouldEnforceCanMakePaymentQueryQuota()) {
            onHasEnrolledInstrument(HasEnrolledInstrumentQueryResult.QUERY_QUOTA_EXCEEDED);
        } else {
            onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.WARNING_HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.WARNING_HAS_NO_ENROLLED_INSTRUMENT);
        }

        mJourneyLogger.setHasEnrolledInstrumentValue(response || mIsOffTheRecord);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onHasEnrolledInstrumentReturned();
        }
    }

    /** @return Whether the instrument has been enrolled. */
    public boolean getHasEnrolledInstrument() {
        return mHasEnrolledInstrument;
    }

    /** Sets whether the instrument has been enrolled. */
    public void setHasEnrolledInstrument(boolean hasEnrolledInstrument) {
        mHasEnrolledInstrument = hasEnrolledInstrument;
    }

    /**
     * Sets the result of CanMakePayment request.
     * @param canMakePayment Whether the user can make a payment with the merchant specified
     *         request.
     */
    public void setCanMakePayment(boolean canMakePayment) {
        mCanMakePayment = canMakePayment;
    }

    /** @return The result of the CanMakePayment request. */
    public boolean getCanMakePayment() {
        return mCanMakePayment;
    }

    /** @return Whether the created payment apps includes any autofill payment app. */
    public boolean getHasNonAutofillApp() {
        return mHasNonAutofillApp;
    }

    /**
     * @return The queryForQuota, a mapping of the payment method names to the corresponding payment
     *         method specific data
     */
    public Map<String, PaymentMethodData> getQueryForQuota() {
        return mQueryForQuota;
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
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.show(isUserGesture, waitForUpdatedDetails);
    }

    /**
     * The component part of the {@link PaymentRequest#updateWith} implementation.
     * @param details The details that the merchant provides to update the payment request.
     */
    /* package */ void updateWith(PaymentDetails details) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.updateWith(details);
    }

    /**
     * The component part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     */
    /* package */ void onPaymentDetailsNotUpdated() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.onPaymentDetailsNotUpdated();
    }

    /** The component part of the {@link PaymentRequest#abort} implementation. */
    /* package */ void abort() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;
        mBrowserPaymentRequest.abort();
    }

    /** The component part of the {@link PaymentRequest#complete} implementation. */
    /* package */ void complete(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.complete(result);
    }

    /**
     * The component part of the {@link PaymentRequest#retry} implementation. Check {@link
     * PaymentRequest#retry} for the parameters' specification.
     */
    /* package */ void retry(PaymentValidationErrors errors) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.retry(errors);
    }

    /** The component part of the {@link PaymentRequest#canMakePayment} implementation. */
    /* package */ void canMakePayment() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.canMakePayment();
    }

    /**
     * The component part of the {@link PaymentRequest#hasEnrolledInstrument} implementation.
     */
    /* package */ void hasEnrolledInstrument() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.hasEnrolledInstrument();
    }

    /**
     * Implement {@link PaymentRequest#close}. This should be called by the renderer only. The
     * closing triggered by other classes should call {@link #close} instead. The caller should
     * stop referencing this class after calling this method.
     */
    /* package */ void closeByRenderer() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

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
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

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
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
        mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
                debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    /**
     * Close this instance and release all of the retained resources. The external callers of this
     * method should stop referencing this instance upon calling. This method can be called within
     * itself without causing infinite loops.
     */
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.close();
        mBrowserPaymentRequest = null;

        // mClient can be null only when this method is called from
        // PaymentRequestService#create().
        if (mClient != null) mClient.close();
        mClient = null;

        mOnClosedListener.run();
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

    /** Invokes {@link PaymentRequestClient.onPaymentMethodChange}. */
    public void onPaymentMethodChange(String methodName, String stringifiedDetails) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
    }

    /** Invokes {@link PaymentRequestClient.onShippingAddressChange}. */
    public void onShippingAddressChange(PaymentAddress address) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        redactShippingAddress(address);
        mClient.onShippingAddressChange(address);
    }

    /** Invokes {@link PaymentRequestClient.onShippingOptionChange}. */
    public void onShippingOptionChange(String shippingOptionId) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onShippingOptionChange(shippingOptionId);
    }

    /** Invokes {@link PaymentRequestClient.onPayerDetailChange}. */
    public void onPayerDetailChange(PayerDetail detail) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPayerDetailChange(detail);
    }

    /** Invokes {@link PaymentRequestClient.onPaymentResponse}. */
    public void onPaymentResponse(PaymentResponse response) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPaymentResponse(response);
    }

    /** Invokes {@link PaymentRequestClient.onError}. */
    public void onError(int error, String errorMessage) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onError(error, errorMessage);
    }

    /** Invokes {@link PaymentRequestClient.onComplete}. */
    public void onComplete() {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onComplete();
    }

    /** Invokes {@link PaymentRequestClient.onAbort}. */
    public void onAbort(boolean abortedSuccessfully) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onAbort(abortedSuccessfully);
    }

    /** Invokes {@link PaymentRequestClient.onCanMakePayment}. */
    public void onCanMakePayment(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onCanMakePayment(result);
    }

    /** Invokes {@link PaymentRequestClient.onHasEnrolledInstrument}. */
    public void onHasEnrolledInstrument(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onHasEnrolledInstrument(result);
    }

    /** Invokes {@link PaymentRequestClient.warnNoFavicon}. */
    public void warnNoFavicon() {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.warnNoFavicon();
    }

    /**
     * @return The logger of the user journey of the Android PaymentRequest service, cannot be
     *         null.
     */
    public JourneyLogger getJourneyLogger() {
        return mJourneyLogger;
    }

    /** @return The WebContents of the merchant's page, cannot be null. */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /** @return Whether the WebContents is currently showing an off-the-record tab. */
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    /** @return The certificate chain from the merchant page's WebContents, can be null. */
    @Nullable
    public byte[][] getCertificateChain() {
        return mCertificateChain;
    }

    /** @return The origin of the page at which the PaymentRequest is created. */
    public String getPaymentRequestOrigin() {
        return mPaymentRequestOrigin;
    }

    /** @return The merchant page's title. */
    public String getMerchantName() {
        return mMerchantName;
    }

    /** @return The origin of the top level frame of the merchant page. */
    public String getTopLevelOrigin() {
        return mTopLevelOrigin;
    }

    /** @return The payment options requested by the merchant, can be null. */
    @Nullable
    public PaymentOptions getPaymentOptions() {
        return mPaymentOptions;
    }

    /** @return The RendererFrameHost of the merchant page. */
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    /**
     * @return The origin of the iframe that invoked the PaymentRequest API. Can be opaque. Used by
     * security features like 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'. Should not be
     * null.
     */
    public Origin getPaymentRequestSecurityOrigin() {
        return mPaymentRequestSecurityOrigin;
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
}
