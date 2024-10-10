// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationAuthnController;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationNoMatchingCredController;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentComplete;
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
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * {@link PaymentRequestService}, {@link MojoPaymentRequestGateKeeper} and
 * ChromePaymentRequestService together make up the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This class provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.ChromePaymentRequestService.
 *
 * <p>TODO(crbug.com/40138829): ChromePaymentRequestService is under refactoring, with the purpose
 * of moving the business logic of ChromePaymentRequestService into PaymentRequestService and
 * eventually moving ChromePaymentRequestService. Note that the callers of the instances of this
 * class need to close them with {@link PaymentRequestService#close()}, after which no usage is
 * allowed.
 */
public class PaymentRequestService
        implements PaymentAppFactoryDelegate,
                PaymentAppFactoryParams,
                PaymentRequestUpdateEventListener,
                PaymentApp.AbortCallback,
                PaymentApp.InstrumentDetailsCallback,
                PaymentDetailsConverter.MethodChecker,
                PaymentResponseHelperInterface.PaymentResponseResultCallback,
                CSPChecker {
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
    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final List<PaymentApp> mPendingApps = new ArrayList<>();
    private final Supplier<PaymentAppServiceBridge> mPaymentAppServiceBridgeSupplier;
    private WebContents mWebContents;
    private JourneyLogger mJourneyLogger;
    private String mTopLevelOrigin;
    private String mPaymentRequestOrigin;
    private Origin mPaymentRequestSecurityOrigin;
    private String mMerchantName;
    @Nullable private byte[][] mCertificateChain;
    private boolean mIsOffTheRecord;
    private PaymentOptions mPaymentOptions;
    private boolean mRequestShipping;
    private boolean mRequestPayerName;
    private boolean mRequestPayerPhone;
    private boolean mRequestPayerEmail;
    private int mShippingType;
    private PaymentRequestSpec mSpec;
    private boolean mHasClosed;
    private boolean mIsFinishedQueryingPaymentApps;
    private boolean mIsShowCalled;
    private boolean mIsShowWaitingForUpdatedDetails;

    /** If not empty, use this error message for rejecting PaymentRequest.show(). */
    private String mRejectShowErrorMessage;

    /** Internal reason for why PaymentRequest.show() should be rejected. */
    private @AppCreationFailureReason int mRejectShowErrorReason = AppCreationFailureReason.UNKNOWN;

    // mClient is null only when it has closed.
    @Nullable private PaymentRequestClient mClient;

    // mBrowserPaymentRequest is null when it has closed or is uninitiated.
    @Nullable private BrowserPaymentRequest mBrowserPaymentRequest;

    /** The helper to create and fill the response to send to the merchant. */
    @Nullable private PaymentResponseHelperInterface mPaymentResponseHelper;

    // mSpcAuthnUiController is null when it is closed and before it is shown.
    @Nullable private SecurePaymentConfirmationAuthnController mSpcAuthnUiController;

    // mNoMatchingController is null when it is closed and before it is shown.
    @Nullable private SecurePaymentConfirmationNoMatchingCredController mNoMatchingController;

    /** A mapping of the payment method names to the corresponding payment method specific data. */
    private HashMap<String, PaymentMethodData> mQueryForQuota;

    /**
     * True after at least one usable payment app has been found and the setting allows querying
     * this value. This value can be used to respond to hasEnrolledInstrument(). Should be read only
     * after all payment apps have been queried.
     */
    private boolean mHasEnrolledInstrument;

    /** True if any of the requested payment methods are supported. */
    private boolean mCanMakePayment;

    /** True if canMakePayment() and hasEnrolledInstrument() are forced to return true. */
    private boolean mCanMakePaymentEvenWithoutApps;

    private boolean mIsCanMakePaymentResponsePending;
    private boolean mIsHasEnrolledInstrumentResponsePending;
    @Nullable private PaymentApp mInvokedPaymentApp;

    /** True if a show() call is rejected for lack of a user activation. */
    private boolean mRejectShowForUserActivation;

    /**
     * An observer interface injected when running tests to allow them to observe events. This
     * interface holds events that should be passed back to the native C++ test harness and mirrors
     * the C++ PaymentRequest::ObserverForTest() interface. Its methods should be called in the same
     * places that the C++ PaymentRequest object will call its ObserverForTest.
     */
    public interface NativeObserverForTest {
        void onCanMakePaymentCalled();

        void onCanMakePaymentReturned();

        void onHasEnrolledInstrumentCalled();

        void onHasEnrolledInstrumentReturned();

        void onAppListReady(@Nullable List<PaymentApp> paymentApps, PaymentItem total);

        void onShippingSectionVisibilityChange(boolean isShippingSectionVisible);

        void onContactSectionVisibilityChange(boolean isContactSectionVisible);

        void onErrorDisplayed();

        void onNotSupportedError();

        void onConnectionTerminated();

        void onAbortCalled();

        void onCompleteHandled();

        void onUiDisplayed();

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
         * Creates an instance of BrowserPaymentRequest.
         *
         * @param paymentRequestService The PaymentRequestService that it depends on.
         * @return The instance.
         */
        BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService);

        /**
         * @return Whether the merchant's WebContents is currently showing an off-the-record tab.
         *     Return true if the tab profile is not accessible from the WebContents.
         */
        boolean isOffTheRecord();

        /**
         * @return A non-null string if there is an invalid SSL certificate on the currently loaded
         *     page.
         */
        String getInvalidSslCertificateErrorMessage();

        /**
         * @return Whether the preferences allow CAN_MAKE_PAYMENT.
         */
        boolean prefsCanMakePayment();

        /**
         * @return If the merchant's WebContents is running inside of a Trusted Web Activity,
         *     returns the package name for Trusted Web Activity. Otherwise returns an empty string
         *     or null.
         */
        @Nullable
        String getTwaPackageName();

        /**
         * Gets the WebContents from a RenderFrameHost if the WebContents has not been destroyed;
         * otherwise, return null.
         *
         * @param renderFrameHost The {@link RenderFrameHost} of any frame in which the intended
         *     WebContents contains.
         * @return The WebContents.
         */
        @Nullable
        default WebContents getLiveWebContents(RenderFrameHost renderFrameHost) {
            return PaymentRequestServiceUtil.getLiveWebContents(renderFrameHost);
        }

        /**
         * Returns true for a valid URL from a secure origin.
         *
         * @param url The URL to check.
         * @return Whether the origin of the URL is secure.
         */
        default boolean isOriginSecure(GURL url) {
            return OriginSecurityChecker.isOriginSecure(url);
        }

        /**
         * Creates a journey logger.
         *
         * @param webContents The web contents where PaymentRequest API is invoked. Should not be
         *     null.
         */
        default JourneyLogger createJourneyLogger(WebContents webContents) {
            return new JourneyLogger(webContents);
        }

        /**
         * Builds a String that strips down |uri| to its scheme, host, and port.
         *
         * @param uri The URI to break down.
         * @return Stripped-down String containing the essential bits of the URL, or the original
         *     URL if it fails to parse it.
         */
        default String formatUrlForSecurityDisplay(GURL uri) {
            return UrlFormatter.formatUrlForSecurityDisplay(uri, SchemeDisplay.SHOW);
        }

        /**
         * @param webContents The WebContents to get site certificate chain from.
         * @return The site certificate chain of the given WebContents.
         */
        default byte[][] getCertificateChain(WebContents webContents) {
            return CertificateChainHelper.getCertificateChain(webContents);
        }

        /**
         * Checks whether the page at the given URL should be allowed to use the web payment APIs.
         *
         * @param url The URL to check.
         * @return Whether the page is allowed to use web payment APIs.
         */
        default boolean isOriginAllowedToUseWebPaymentApis(GURL url) {
            return UrlUtil.isOriginAllowedToUseWebPaymentApis(url);
        }

        /**
         * @param details The payment details to verify.
         * @return Whether the details are valid.
         */
        default boolean validatePaymentDetails(PaymentDetails details) {
            return PaymentValidator.validatePaymentDetails(details);
        }

        /**
         * Creates an instance of {@link PaymentRequestSpec} that stores the given info.
         *
         * @param options The payment options, e.g., whether shipping is requested.
         * @param details The payment details, e.g., the total amount.
         * @param methodData The list of supported payment method identifiers and corresponding
         *     payment method specific data.
         * @param appLocale The current application locale.
         * @return The payment request spec.
         */
        default PaymentRequestSpec createPaymentRequestSpec(
                PaymentOptions options,
                PaymentDetails details,
                Collection<PaymentMethodData> methodData,
                String appLocale) {
            return new PaymentRequestSpec(options, details, methodData, appLocale);
        }

        /**
         * @return The PaymentAppService that is used to create payment app for this service.
         */
        default PaymentAppService getPaymentAppService() {
            return PaymentAppService.getInstance();
        }

        /**
         * Creates an instance of Android payment app factory.
         *
         * @return The instance, can be null for testing.
         */
        @Nullable
        default PaymentAppFactoryInterface createAndroidPaymentAppFactory() {
            return new AndroidPaymentAppFactory();
        }

        /**
         * @return The context of the current activity, can be null when WebContents has been
         *     destroyed, the activity is gone, the window is closed, etc.
         */
        @Nullable
        default Context getContext(RenderFrameHost renderFrameHost) {
            WindowAndroid window = getWindowAndroid(renderFrameHost);
            if (window == null) return null;
            return window.getContext().get();
        }

        /**
         * @return The WindowAndroid of the current activity, can be null when WebContents has been
         *     destroyed, the activity is gone, etc.
         */
        @Nullable
        default WindowAndroid getWindowAndroid(RenderFrameHost renderFrameHost) {
            WebContents webContents = PaymentRequestServiceUtil.getLiveWebContents(renderFrameHost);
            if (webContents == null) return null;
            return webContents.getTopLevelNativeWindow();
        }
    }

    /** A test-only observer for the PaymentRequest service implementation. */
    public interface PaymentRequestServiceObserverForTest {
        /** Called when an abort request was denied. */
        void onPaymentRequestServiceUnableToAbort();

        /**
         * Called when the controller is notified of billing address change, but does not alter the
         * editor UI.
         */
        void onPaymentRequestServiceBillingAddressChangeProcessed();

        /** Called when the controller is notified of an expiration month change. */
        void onPaymentRequestServiceExpirationMonthChange();

        /**
         * Called when a show request failed. This can happen when:
         *
         * <ul>
         *   <li>The merchant requests only unsupported payment methods.
         *   <li>The merchant requests only payment methods that don't have corresponding apps and
         *       are not able to add a credit card from PaymentRequest UI.
         * </ul>
         */
        void onPaymentRequestServiceShowFailed();

        /** Called when the canMakePayment() request has been responded to. */
        void onPaymentRequestServiceCanMakePaymentQueryResponded();

        /** Called when the hasEnrolledInstrument() request has been responded to. */
        void onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();

        /** Called when the payment response is ready. */
        void onPaymentResponseReady();

        /**
         * Called when the browser has handled the renderer's complete call, which indicates that
         * the browser UI has closed.
         */
        void onCompletedHandled();

        /**
         * Called when the renderer is closing the mojo connection (e.g. upon show promise
         * rejection).
         */
        void onRendererClosedMojoConnection();
    }

    /**
     * Creates an instance of the class.
     *
     * @param renderFrameHost The RenderFrameHost of the merchant page.
     * @param client The client of the renderer PaymentRequest, can be null.
     * @param onClosedListener A listener to be invoked when the service is closed.
     * @param delegate The delegate of this class.
     * @param paymentAppServiceBridgeSupplier The supplier of PaymentAppServiceBridge - a C++
     *     factory that creates service-worker payment apps, secure payment confirmation apps, etc.
     */
    public PaymentRequestService(
            RenderFrameHost renderFrameHost,
            @Nullable PaymentRequestClient client,
            Runnable onClosedListener,
            Delegate delegate,
            Supplier<PaymentAppServiceBridge> paymentAppServiceBridgeSupplier) {
        assert renderFrameHost != null;
        assert onClosedListener != null;
        assert delegate != null;

        mRenderFrameHost = renderFrameHost;
        mClient = client;
        mOnClosedListener = onClosedListener;
        mDelegate = delegate;
        mHasClosed = false;
        mPaymentAppServiceBridgeSupplier = paymentAppServiceBridgeSupplier;
        mRejectShowForUserActivation = false;
    }

    /**
     * Initializes the payment request service.
     *
     * @param methodData The supported methods specified by the merchant, need validation before
     *     usage, can be null.
     * @param details The payment details specified by the merchant, need validation before usage,
     *     can be null.
     * @param options The payment options specified by the merchant, need validation before usage,
     *     can be null.
     * @return Whether the initialization is successful.
     */
    public boolean init(
            @Nullable PaymentMethodData[] rawMethodData,
            @Nullable PaymentDetails details,
            @Nullable PaymentOptions options) {
        if (mRenderFrameHost.getLastCommittedOrigin() == null
                || mRenderFrameHost.getLastCommittedURL() == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.NO_FRAME);
            return false;
        }
        mPaymentRequestSecurityOrigin = mRenderFrameHost.getLastCommittedOrigin();
        // TODO(crbug.com/41475385): replace UrlFormatter with GURL operations.
        mPaymentRequestOrigin =
                mDelegate.formatUrlForSecurityDisplay(mRenderFrameHost.getLastCommittedURL());

        mWebContents = mDelegate.getLiveWebContents(mRenderFrameHost);
        if (mWebContents == null || mWebContents.isDestroyed()) {
            abortForInvalidDataFromRenderer(ErrorStrings.NO_WEB_CONTENTS);
            return false;
        }
        // TODO(crbug.com/41475385): replace UrlFormatter with GURL operations.
        mTopLevelOrigin = mDelegate.formatUrlForSecurityDisplay(mWebContents.getLastCommittedUrl());

        mMerchantName = mWebContents.getTitle();
        mCertificateChain = mDelegate.getCertificateChain(mWebContents);
        mIsOffTheRecord = mDelegate.isOffTheRecord();
        mJourneyLogger = mDelegate.createJourneyLogger(mWebContents);

        if (mClient == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_STATE);
            return false;
        }

        if (!mDelegate.isOriginSecure(mWebContents.getLastCommittedUrl())) {
            abortForInvalidDataFromRenderer(ErrorStrings.NOT_IN_A_SECURE_ORIGIN);
            return false;
        }

        if (rawMethodData == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA);
            return false;
        }

        // details has default value, so could never be null, according to payment_request.idl.
        if (details == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return false;
        }

        // options has default value, so could never be null, according to
        // payment_request.idl.
        if (options == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_PAYMENT_OPTIONS);
            return false;
        }
        mPaymentOptions = options;
        mRequestShipping = mPaymentOptions.requestShipping;
        mRequestPayerName = mPaymentOptions.requestPayerName;
        mRequestPayerPhone = mPaymentOptions.requestPayerPhone;
        mRequestPayerEmail = mPaymentOptions.requestPayerEmail;
        mShippingType = mPaymentOptions.shippingType;

        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.INITIATED);

        if (!mDelegate.isOriginAllowedToUseWebPaymentApis(mWebContents.getLastCommittedUrl())) {
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN);
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
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
            disconnectFromClientWithDebugMessage(
                    rejectShowErrorMessage,
                    PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
            return false;
        }

        mBrowserPaymentRequest = mDelegate.createBrowserPaymentRequest(this);
        @Nullable Map<String, PaymentMethodData> methodData = getValidatedMethodData(rawMethodData);
        if (methodData == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                    PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return false;
        }
        if (methodData.containsKey(MethodStrings.SECURE_PAYMENT_CONFIRMATION)
                && !isValidSecurePaymentConfirmationRequest(methodData, options)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA,
                    PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return false;
        }
        mBrowserPaymentRequest.modifyMethodDataIfNeeded(methodData);
        methodData = Collections.unmodifiableMap(methodData);

        mQueryForQuota = new HashMap<>(methodData);

        if (details.id == null
                || details.total == null
                || !mDelegate.validatePaymentDetails(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS,
                    PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return false;
        }

        if (mBrowserPaymentRequest.disconnectIfExtraValidationFails(
                mWebContents, methodData, details, mPaymentOptions)) {
            return false;
        }

        PaymentRequestSpec spec =
                mDelegate.createPaymentRequestSpec(
                        mPaymentOptions,
                        details,
                        methodData.values(),
                        LocaleUtils.getDefaultLocaleString());
        if (spec.getRawTotal() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.TOTAL_REQUIRED, PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return false;
        }
        mSpec = spec;
        mBrowserPaymentRequest.onSpecValidated(mSpec);
        logRequestedMethods();
        startPaymentAppService();
        return true;
    }

    private boolean isValidSecurePaymentConfirmationRequest(
            Map<String, PaymentMethodData> methodData, PaymentOptions options) {
        if (methodData.size() > 1) return false;
        if (options.requestPayerEmail
                || options.requestPayerPhone
                || options.requestShipping
                || options.requestPayerName) {
            return false;
        }
        PaymentMethodData spcMethodData = methodData.get(MethodStrings.SECURE_PAYMENT_CONFIRMATION);
        if (spcMethodData.securePaymentConfirmation == null) return false;

        // TODO(crbug.com/40231121): Update checks to match desktop browser-side logic.
        if ((spcMethodData.securePaymentConfirmation.payeeOrigin == null
                        && spcMethodData.securePaymentConfirmation.payeeName == null)
                || (spcMethodData.securePaymentConfirmation.payeeName != null
                        && spcMethodData.securePaymentConfirmation.payeeName.isEmpty())) {
            return false;
        }

        if (spcMethodData.securePaymentConfirmation.payeeOrigin != null) {
            Origin origin = new Origin(spcMethodData.securePaymentConfirmation.payeeOrigin);
            if (origin.isOpaque()) return false;
            if (!"https".equals(origin.getScheme())) return false;
        }

        return true;
    }

    private void startPaymentAppService() {
        PaymentAppService service = mDelegate.getPaymentAppService();

        String paymentAppServiceBridgeId = PaymentAppServiceBridge.class.getName();
        if (!service.containsFactory(paymentAppServiceBridgeId)) {
            service.addUniqueFactory(
                    mPaymentAppServiceBridgeSupplier.get(), paymentAppServiceBridgeId);
        }

        String androidFactoryId = AndroidPaymentAppFactory.class.getName();
        if (!service.containsFactory(androidFactoryId)) {
            service.addUniqueFactory(mDelegate.createAndroidPaymentAppFactory(), androidFactoryId);
        }

        service.create(/* delegate= */ this);
    }

    /**
     * @return Whether the payment details is pending to be updated due to a promise that was passed
     *     into PaymentRequest.show().
     */
    public boolean isShowWaitingForUpdatedDetails() {
        return mIsShowWaitingForUpdatedDetails;
    }

    /**
     * Called to open a new PaymentHandler UI on the showing PaymentRequest.
     *
     * @param url The url of the payment app to be displayed in the UI.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *     successful; null if failed.
     */
    @Nullable
    public static WebContents openPaymentHandlerWindow(GURL url) {
        if (sShowingPaymentRequest == null) return null;
        PaymentApp invokedPaymentApp = sShowingPaymentRequest.mInvokedPaymentApp;
        assert invokedPaymentApp != null;
        assert invokedPaymentApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP;
        return sShowingPaymentRequest.mBrowserPaymentRequest.openPaymentHandlerWindow(
                url, invokedPaymentApp.getUkmSourceId());
    }

    /**
     * Disconnects from the PaymentRequestClient with a debug message.
     *
     * @param debugMessage The debug message shown for web developers.
     * @param reason The reason of the disconnection defined in {@link PaymentErrorReason}.
     */
    public void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mClient != null) {
            // Secure Payment Confirmation must make it indistinguishable to the merchant page as to
            // whether an error is caused by user aborting or lack of credentials. There are three
            // exceptions:
            //
            //   1. Erroring due to icon download failure; this happens before checking for
            //      credential matching and so is not a privacy leak.
            //   2. Handling the 'opt out' error - this error can be produced by both the matching
            //      and non-matching credential UXs, and so is not a privacy leak.
            //   3. Erroring due to a lack of user activation when it is not allowed.
            boolean obscureRealError =
                    mSpec != null
                            && mSpec.isSecurePaymentConfirmationRequested()
                            && mRejectShowErrorReason
                                    != AppCreationFailureReason.ICON_DOWNLOAD_FAILED
                            && reason != PaymentErrorReason.USER_OPT_OUT
                            && !mRejectShowForUserActivation;
            mClient.onError(
                    obscureRealError ? PaymentErrorReason.NOT_ALLOWED_ERROR : reason,
                    obscureRealError
                            ? ErrorStrings.WEB_AUTHN_OPERATION_TIMED_OUT_OR_NOT_ALLOWED
                            : debugMessage);
        }
        close();
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Set a native-side observer for PaymentRequest implementations. This observer should be set
     * before PaymentRequest implementations are instantiated.
     *
     * @param nativeObserverForTest The native-side observer.
     */
    public static void setNativeObserverForTest(NativeObserverForTest nativeObserverForTest) {
        sNativeObserverForTest = nativeObserverForTest;
        ResettersForTesting.register(() -> sNativeObserverForTest = null);
    }

    /**
     * @return Get the native=side observer, for testing purpose only.
     */
    @Nullable
    public static NativeObserverForTest getNativeObserverForTest() {
        return sNativeObserverForTest;
    }

    private void logRequestedMethods() {
        List<Integer> methodTypes = new ArrayList<>();
        for (String methodName : mSpec.getMethodData().keySet()) {
            switch (methodName) {
                case MethodStrings.ANDROID_PAY:
                case MethodStrings.GOOGLE_PAY:
                    methodTypes.add(PaymentMethodCategory.GOOGLE);
                    break;
                case MethodStrings.GOOGLE_PAY_AUTHENTICATION:
                    methodTypes.add(PaymentMethodCategory.GOOGLE_PAY_AUTHENTICATION);
                    break;
                case MethodStrings.GOOGLE_PLAY_BILLING:
                    methodTypes.add(PaymentMethodCategory.PLAY_BILLING);
                    break;
                case MethodStrings.SECURE_PAYMENT_CONFIRMATION:
                    methodTypes.add(PaymentMethodCategory.SECURE_PAYMENT_CONFIRMATION);
                    break;
                case MethodStrings.BASIC_CARD:
                    // Not to record requestedMethodBasicCard because JourneyLogger ignore the case
                    // where the specified networks are unsupported.
                    // mPaymentUiService.merchantSupportsAutofillCards() better captures this group
                    // of interest than requestedMethodBasicCard.
                    break;
                default:
                    // "Other" includes https url, http url(when certificate check is bypassed) and
                    // the unlisted methods defined in {@link MethodStrings}.
                    methodTypes.add(PaymentMethodCategory.OTHER);
            }
        }

        mJourneyLogger.setRequestedPaymentMethods(methodTypes);
    }

    // Implements PaymentResponseHelper.PaymentResponseResultCallback:
    @Override
    public void onPaymentResponseReady(PaymentResponse response) {
        if (!mBrowserPaymentRequest.patchPaymentResponseIfNeeded(response)) {
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.PAYMENT_APP_INVALID_RESPONSE, PaymentErrorReason.NOT_SUPPORTED);
            // Intentionally do not early-return.
        }
        if (response.methodName.equals(MethodStrings.SECURE_PAYMENT_CONFIRMATION)) {
            assert mInvokedPaymentApp.getInstrumentMethodNames().contains(response.methodName);
            response = mInvokedPaymentApp.setAppSpecificResponseFields(response);
        }
        if (mClient != null) {
            mClient.onPaymentResponse(response);
        }
        mPaymentResponseHelper = null;
        if (sObserverForTest != null) {
            sObserverForTest.onPaymentResponseReady();
        }
    }

    // Implements CSPChecker:
    @Override
    public void allowConnectToSource(
            GURL url,
            GURL urlBeforeRedirects,
            boolean didFollowRedirect,
            Callback<Boolean> resultCallback) {
        if (mClient == null) return;
        mClient.allowConnectToSource(
                url.toMojom(),
                urlBeforeRedirects.toMojom(),
                didFollowRedirect,
                (allow) -> {
                    resultCallback.onResult(allow);
                });
    }

    /**
     * Invokes the given payment app.
     *
     * @param paymentApp The payment app to be invoked.
     * @param paymentResponseHelper The helper to create and fill the response to send to the
     *     merchant. The helper should have this instance as the delegate {@link
     *     PaymentResponseHelperInterface.PaymentResponseResultCallback}.
     */
    public void invokePaymentApp(
            PaymentApp paymentApp, PaymentResponseHelperInterface paymentResponseHelper) {
        if (paymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance()
                    .initialize(
                            new PackageManagerDelegate(),
                            ((AndroidPaymentApp) paymentApp).packageName(),
                            this /* PaymentApp.PaymentRequestUpdateEventListener */);
        }
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
        paymentOptions.shippingType =
                mRequestShipping && paymentApp.handlesShippingAddress()
                        ? mShippingType
                        : PaymentShippingType.SHIPPING;

        // Redact shipping options if the selected app cannot handle shipping.
        List<PaymentShippingOption> redactedShippingOptions =
                paymentApp.handlesShippingAddress()
                        ? mSpec.getRawShippingOptions()
                        : Collections.unmodifiableList(new ArrayList<>());
        paymentApp.invokePaymentApp(
                mSpec.getId(),
                mMerchantName,
                mTopLevelOrigin,
                mPaymentRequestOrigin,
                mCertificateChain,
                Collections.unmodifiableMap(methodData),
                mSpec.getRawTotal(),
                mSpec.getRawLineItems(),
                Collections.unmodifiableMap(modifiers),
                paymentOptions,
                redactedShippingOptions,
                /* callback= */ this);
        mInvokedPaymentApp = paymentApp;
        mJourneyLogger.setPayClicked();
        logSelectedMethod(paymentApp);
    }

    private void logSelectedMethod(PaymentApp invokedPaymentApp) {
        @PaymentMethodCategory int category = PaymentMethodCategory.OTHER;
        for (String method : invokedPaymentApp.getInstrumentMethodNames()) {
            if (method.equals(MethodStrings.ANDROID_PAY)
                    || method.equals(MethodStrings.GOOGLE_PAY)) {
                category = PaymentMethodCategory.GOOGLE;
                break;
            } else if (method.equals(MethodStrings.GOOGLE_PAY_AUTHENTICATION)) {
                category = PaymentMethodCategory.GOOGLE_PAY_AUTHENTICATION;
                break;
            } else if (method.equals(MethodStrings.GOOGLE_PLAY_BILLING)) {
                assert invokedPaymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP;
                category = PaymentMethodCategory.PLAY_BILLING;
                break;
            } else if (method.equals(MethodStrings.SECURE_PAYMENT_CONFIRMATION)) {
                assert invokedPaymentApp.getPaymentAppType() == PaymentAppType.INTERNAL;
                category = PaymentMethodCategory.SECURE_PAYMENT_CONFIRMATION;
                break;
            }
        }

        mJourneyLogger.setSelectedMethod(category);
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void setCanMakePaymentEvenWithoutApps() {
        mCanMakePaymentEvenWithoutApps = true;
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory /* Unused */) {
        if (mBrowserPaymentRequest == null) return;
        assert mSpec != null;
        assert !mSpec.isDestroyed() : "mSpec is destroyed only after close()";

        mIsFinishedQueryingPaymentApps = true;

        mHasEnrolledInstrument |= mCanMakePaymentEvenWithoutApps;
        // The kCanMakePaymentEnabled pref does not apply to SPC, where hasEnrolledInstrument() is
        // only used for feature detection and does not communicate with any applications.
        mHasEnrolledInstrument &=
                (mDelegate.prefsCanMakePayment() || mSpec.isSecurePaymentConfirmationRequested());

        mBrowserPaymentRequest.notifyPaymentUiOfPendingApps(mPendingApps);
        mPendingApps.clear();
        // Record the number suggested payment methods and whether at least one of them was
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(
                Section.PAYMENT_METHOD,
                mBrowserPaymentRequest.getPaymentApps().size(),
                mBrowserPaymentRequest.hasAnyCompleteApp());
        if (mIsShowCalled) {
            PaymentNotShownError notShownError = onShowCalledAndAppsQueried();
            if (notShownError != null) {
                onShowFailed(notShownError);
                return;
            }
        }

        if (mIsCanMakePaymentResponsePending) {
            respondCanMakePaymentQuery();
        }

        if (mIsHasEnrolledInstrumentResponsePending) {
            respondHasEnrolledInstrumentQuery();
        }
    }

    @Nullable
    private PaymentNotShownError onShowCalledAndAppsQueried() {
        assert mIsShowCalled;
        assert mIsFinishedQueryingPaymentApps;
        assert mBrowserPaymentRequest != null;

        if (mSpec != null
                && !mSpec.isDestroyed()
                && mSpec.isSecurePaymentConfirmationRequested()
                && !mBrowserPaymentRequest.hasAvailableApps()
                // In most cases, we show the 'No Matching Payment Credential' dialog in order to
                // preserve user privacy. An exception is failure to download the card art icon -
                // because we download it in all cases, revealing a failure doesn't leak any
                // information about the user to the site.
                && mRejectShowErrorReason != AppCreationFailureReason.ICON_DOWNLOAD_FAILED
                // Another exception is if the show() request is being denied for lack of a user
                // gesture.
                && !mRejectShowForUserActivation) {
            mJourneyLogger.setNoMatchingCredentialsShown();
            mNoMatchingController =
                    SecurePaymentConfirmationNoMatchingCredController.create(mWebContents);
            Runnable continueCallback =
                    () -> {
                        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
                        disconnectFromClientWithDebugMessage(
                                ErrorStrings.WEB_AUTHN_OPERATION_TIMED_OUT_OR_NOT_ALLOWED,
                                PaymentErrorReason.NOT_ALLOWED_ERROR);
                    };
            Runnable optOutCallback =
                    () -> {
                        mJourneyLogger.setAborted(AbortReason.USER_OPTED_OUT);
                        disconnectFromClientWithDebugMessage(
                                ErrorStrings.SPC_USER_OPTED_OUT, PaymentErrorReason.USER_OPT_OUT);
                    };
            PaymentMethodData spcMethodData =
                    mSpec.getMethodData().get(MethodStrings.SECURE_PAYMENT_CONFIRMATION);
            assert spcMethodData != null;
            mNoMatchingController.show(
                    continueCallback,
                    optOutCallback,
                    spcMethodData.securePaymentConfirmation.showOptOut,
                    spcMethodData.securePaymentConfirmation.rpId);
            if (sNativeObserverForTest != null) sNativeObserverForTest.onErrorDisplayed();
            return null;
        }

        PaymentNotShownError ensureError = ensureHasSupportedPaymentMethods();
        if (ensureError != null) return ensureError;
        // Send AppListReady signal when all apps are created and request.show() is called.
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onShippingSectionVisibilityChange(
                    mBrowserPaymentRequest.isShippingSectionVisible());
            sNativeObserverForTest.onContactSectionVisibilityChange(
                    mBrowserPaymentRequest.isContactSectionVisible());
            sNativeObserverForTest.onAppListReady(
                    mBrowserPaymentRequest.getPaymentApps(), mSpec.getRawTotal());
        }
        boolean shouldSkip = shouldSkipAppSelector();
        String showError =
                mBrowserPaymentRequest.showOrSkipAppSelector(
                        mIsShowWaitingForUpdatedDetails, mSpec.getRawTotal(), shouldSkip);
        if (showError != null) {
            return new PaymentNotShownError(showError, PaymentErrorReason.NOT_SUPPORTED);
        }

        if (mIsShowWaitingForUpdatedDetails) return null;
        String error = onShowCalledAndAppsQueriedAndDetailsFinalized();
        if (error != null) {
            return new PaymentNotShownError(error, PaymentErrorReason.NOT_SUPPORTED);
        }

        return null;
    }

    // Returns the error if any.
    @Nullable
    private String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        assert mSpec.getRawTotal() != null;
        if (isSecurePaymentConfirmationApplicable()) {
            assert mBrowserPaymentRequest.getSelectedPaymentApp() != null;
            assert mSpcAuthnUiController == null;

            mSpcAuthnUiController = SecurePaymentConfirmationAuthnController.create(mWebContents);
            PaymentMethodData spcMethodData =
                    mSpec.getMethodData().get(MethodStrings.SECURE_PAYMENT_CONFIRMATION);
            assert spcMethodData != null;
            Origin payeeOrigin =
                    spcMethodData.securePaymentConfirmation.payeeOrigin != null
                            ? new Origin(spcMethodData.securePaymentConfirmation.payeeOrigin)
                            : null;
            Callback<Boolean> responseCallback =
                    (response) -> {
                        if (response) {
                            onSecurePaymentConfirmationUiAccepted(
                                    mBrowserPaymentRequest.getSelectedPaymentApp());
                        } else {
                            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
                            disconnectFromClientWithDebugMessage(
                                    ErrorStrings.WEB_AUTHN_OPERATION_TIMED_OUT_OR_NOT_ALLOWED,
                                    PaymentErrorReason.NOT_ALLOWED_ERROR);
                        }

                        mSpcAuthnUiController = null;
                    };
            Runnable optOutCallback =
                    () -> {
                        mJourneyLogger.setAborted(AbortReason.USER_OPTED_OUT);
                        disconnectFromClientWithDebugMessage(
                                ErrorStrings.SPC_USER_OPTED_OUT, PaymentErrorReason.USER_OPT_OUT);
                        mSpcAuthnUiController = null;
                    };
            boolean success =
                    mSpcAuthnUiController.show(
                            mBrowserPaymentRequest.getSelectedPaymentApp().getDrawableIcon(),
                            mBrowserPaymentRequest.getSelectedPaymentApp().getLabel(),
                            getRawTotal(),
                            responseCallback,
                            optOutCallback,
                            spcMethodData.securePaymentConfirmation.payeeName,
                            payeeOrigin,
                            spcMethodData.securePaymentConfirmation.showOptOut,
                            spcMethodData.securePaymentConfirmation.rpId);

            if (success) {
                mJourneyLogger.setShown();
                if (sNativeObserverForTest != null) {
                    sNativeObserverForTest.onUiDisplayed();
                }
                return null;
            } else {
                mSpcAuthnUiController = null;
                return ErrorStrings.SPC_AUTHN_UI_SUPPRESSED;
            }
        }
        return mBrowserPaymentRequest.onShowCalledAndAppsQueriedAndDetailsFinalized();
    }

    private boolean isSecurePaymentConfirmationApplicable() {
        PaymentApp selectedApp = mBrowserPaymentRequest.getSelectedPaymentApp();
        // TODO(crbug.com/40767878): Deduplicate this part with
        // SecurePaymentConfirmationController::SetupModelAndShowDialogIfApplicable().
        return selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.INTERNAL
                && selectedApp.getInstrumentMethodNames().size() == 1
                && selectedApp
                        .getInstrumentMethodNames()
                        .contains(MethodStrings.SECURE_PAYMENT_CONFIRMATION)
                && mBrowserPaymentRequest.getPaymentApps().size() == 1
                && mSpec != null
                && !mSpec.isDestroyed()
                && mSpec.isSecurePaymentConfirmationRequested()
                && !PaymentOptionsUtils.requestAnyInformation(mSpec.getPaymentOptions());
    }

    private void onSecurePaymentConfirmationUiAccepted(PaymentApp app) {
        PaymentResponseHelperInterface paymentResponseHelper =
                new PaymentResponseHelper(app, mSpec.getPaymentOptions());
        invokePaymentApp(app, paymentResponseHelper);
    }

    private void onShowFailed(String error) {
        onShowFailed(error, PaymentErrorReason.USER_CANCEL);
    }

    private void onShowFailed(PaymentNotShownError error) {
        onShowFailed(error.getErrorMessage(), error.getPaymentErrorReason());
    }

    // paymentErrorReason is defined in PaymentErrorReason.
    private void onShowFailed(String error, int paymentErrorReason) {
        mJourneyLogger.setNotShown();
        disconnectFromClientWithDebugMessage(error, paymentErrorReason);
        if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
    }

    /**
     * Ensures the available payment apps can make payment.
     *
     * @return The error if the payment cannot be made; null otherwise.
     */
    @Nullable
    private PaymentNotShownError ensureHasSupportedPaymentMethods() {
        assert mIsShowCalled;
        assert mIsFinishedQueryingPaymentApps;
        if (!mCanMakePayment || !mBrowserPaymentRequest.hasAvailableApps()) {
            // All factories have responded, but none of them have apps. It's possible to add credit
            // cards, but the merchant does not support them either. The payment request must be
            // rejected.
            String debugMessage;
            int paymentErrorReason;
            if (mDelegate.isOffTheRecord()) {
                // If the user is in the OffTheRecord mode, hide the absence of their payment
                // methods from the merchant site.
                debugMessage = ErrorStrings.USER_CANCELLED;
                paymentErrorReason = PaymentErrorReason.USER_CANCEL;
            } else {
                if (sNativeObserverForTest != null) {
                    sNativeObserverForTest.onNotSupportedError();
                }

                if (TextUtils.isEmpty(mRejectShowErrorMessage)
                        && !isInTwa()
                        && mSpec.getMethodData().get(MethodStrings.GOOGLE_PLAY_BILLING) != null) {
                    mRejectShowErrorMessage = ErrorStrings.APP_STORE_METHOD_ONLY_SUPPORTED_IN_TWA;
                }
                debugMessage =
                        ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet())
                                + (TextUtils.isEmpty(mRejectShowErrorMessage)
                                        ? ""
                                        : " " + mRejectShowErrorMessage);
                paymentErrorReason = PaymentErrorReason.NOT_SUPPORTED;
            }
            return new PaymentNotShownError(debugMessage, paymentErrorReason);
        }
        return null;
    }

    private boolean isInTwa() {
        return !TextUtils.isEmpty(mDelegate.getTwaPackageName());
    }

    public static void setIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest() {
        sIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest = true;
        ResettersForTesting.register(
                () -> sIsLocalHasEnrolledInstrumentQueryQuotaEnforcedForTest = false);
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
        if (!mBrowserPaymentRequest.onPaymentAppCreated(paymentApp)) return;
        mHasEnrolledInstrument |= paymentApp.hasEnrolledInstrument();

        mPendingApps.add(paymentApp);
    }

    /** Responds to the CanMakePayment query from the merchant page. */
    public void respondCanMakePaymentQuery() {
        if (mClient == null) return;

        mIsCanMakePaymentResponsePending = false;

        // The kCanMakePaymentEnabled pref does not apply to SPC, where canMakePayment() is only
        // used for feature detection and does not communicate with any applications.
        boolean allowedByPref = true;
        if (!mSpec.isSecurePaymentConfirmationRequested()) {
            allowedByPref = mDelegate.prefsCanMakePayment();
            RecordHistogram.recordBooleanHistogram(
                    "PaymentRequest.CanMakePayment.CallAllowedByPref", allowedByPref);
        }

        boolean response = mCanMakePayment && allowedByPref;
        mClient.onCanMakePayment(
                response
                        ? CanMakePaymentQueryResult.CAN_MAKE_PAYMENT
                        : CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);

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

        // The pref is checked in onDoneCreatingPaymentApps, but we explicitly want to measure
        // calls to hasEnrolledInstrument() that are affected by it.
        if (!mSpec.isSecurePaymentConfirmationRequested()) {
            RecordHistogram.recordBooleanHistogram(
                    "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref",
                    mDelegate.prefsCanMakePayment());
        }

        boolean response = mHasEnrolledInstrument;
        mIsHasEnrolledInstrumentResponsePending = false;

        int result;
        if (HasEnrolledInstrumentQuery.canQuery(
                mWebContents, mTopLevelOrigin, mPaymentRequestOrigin, mQueryForQuota)) {
            result =
                    response
                            ? HasEnrolledInstrumentQueryResult.HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT;
        } else if (shouldEnforceHasEnrolledInstrumentQueryQuota()) {
            result = HasEnrolledInstrumentQueryResult.QUERY_QUOTA_EXCEEDED;
        } else {
            result =
                    response
                            ? HasEnrolledInstrumentQueryResult.WARNING_HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.WARNING_HAS_NO_ENROLLED_INSTRUMENT;
        }
        mClient.onHasEnrolledInstrument(result);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onHasEnrolledInstrumentReturned();
        }
    }

    /**
     * @return Whether hasEnrolledInstrument() query quota should be enforced. By default, the quota
     *     is enforced only on https:// scheme origins. However, the tests also enable the quota on
     *     localhost and file:// scheme origins to verify its behavior.
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
        mCanMakePayment = canMakePayment || mCanMakePaymentEvenWithoutApps;
        if (!mIsCanMakePaymentResponsePending) return;
        // canMakePayment doesn't need to wait for all apps to be queried because it only needs to
        // test the existence of a payment handler.
        respondCanMakePaymentQuery();
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void onPaymentAppCreationError(
            String errorMessage, @AppCreationFailureReason int errorReason) {
        if (TextUtils.isEmpty(mRejectShowErrorMessage)) {
            mRejectShowErrorMessage = errorMessage;
            mRejectShowErrorReason = errorReason;
        }
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public void setOptOutOffered() {
        mJourneyLogger.setOptOutOffered();
    }

    // Implements PaymentAppFactoryDelegate:
    @Override
    public CSPChecker getCSPChecker() {
        return this;
    }

    /**
     * @param methodDataList A list of PaymentMethodData.
     * @return The validated method data, a mapping of method names to its PaymentMethodData(s);
     *     when the given method data is invalid, returns null.
     */
    @Nullable
    private static Map<String, PaymentMethodData> getValidatedMethodData(
            PaymentMethodData[] methodDataList) {
        // Payment methodData are required.
        assert methodDataList != null;
        if (methodDataList.length == 0) return null;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (PaymentMethodData methodData : methodDataList) {
            if (methodData == null) return null;
            String methodName = methodData.supportedMethod;
            if (TextUtils.isEmpty(methodName)) return null;
            result.put(methodName, methodData);
        }
        return result;
    }

    public static void resetShowingPaymentRequestForTest() {
        sShowingPaymentRequest = null;
    }

    /**
     * The component part of the {@link PaymentRequest#show} implementation. Check {@link
     * PaymentRequest#show} for the parameters' specification.
     */
    /* package */ void show(boolean waitForUpdatedDetails, boolean hadUserActivation) {
        if (mBrowserPaymentRequest == null) return;
        assert mSpec != null;
        assert !mSpec.isDestroyed() : "mSpec is destroyed only after close().";

        if (mIsShowCalled) {
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
            onShowFailed(ErrorStrings.ANOTHER_UI_SHOWING, PaymentErrorReason.ALREADY_SHOWING);
            return;
        }
        if (!hadUserActivation) {
            PaymentRequestWebContentsData paymentRequestWebContentsData =
                    PaymentRequestWebContentsData.from(mWebContents);
            if (paymentRequestWebContentsData.hadActivationlessShow()) {
                // Reject the call to show(), because only one activationless show is allowed per
                // page.
                mRejectShowForUserActivation = true;
                onShowFailed(
                        ErrorStrings.CANNOT_SHOW_WITHOUT_USER_ACTIVATION,
                        PaymentErrorReason.USER_ACTIVATION_REQUIRED);
                return;
            }
            mJourneyLogger.setActivationlessShow();
            paymentRequestWebContentsData.recordActivationlessShow();
        }
        sShowingPaymentRequest = this;
        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.SHOW_CALLED);
        mIsShowCalled = true;
        mIsShowWaitingForUpdatedDetails = waitForUpdatedDetails;

        if (mIsFinishedQueryingPaymentApps) {
            PaymentNotShownError notShownError = onShowCalledAndAppsQueried();
            if (notShownError != null) {
                onShowFailed(notShownError);
                return;
            }
        }
    }

    /**
     * @param options The payment options specified in the payment request.
     * @param allApps All available payment apps.
     * @return true when there is exactly one available payment app which can provide all requested
     *     information including shipping address and payer's contact information whenever needed.
     */
    private static boolean onlySingleAppCanProvideAllRequiredInformation(
            PaymentOptions options, List<PaymentApp> allApps) {
        if (!PaymentOptionsUtils.requestAnyInformation(options)) {
            return allApps.size() == 1;
        }

        boolean anAppCanProvideAllInfo = false;
        for (int i = 0; i < allApps.size(); i++) {
            PaymentApp app = allApps.get(i);
            if ((!options.requestShipping || app.handlesShippingAddress())
                    && (!options.requestPayerName || app.handlesPayerName())
                    && (!options.requestPayerPhone || app.handlesPayerPhone())
                    && (!options.requestPayerEmail || app.handlesPayerEmail())) {
                // There is more than one available app that can provide all merchant requested
                // information information.
                if (anAppCanProvideAllInfo) return false;

                anAppCanProvideAllInfo = true;
            }
        }
        return anAppCanProvideAllInfo;
    }

    /**
     * @param methods The payment methods supported by the payment request.
     * @return True when at least one url payment method identifier is specified in payment request.
     */
    public static boolean isUrlPaymentMethodIdentifiersSupported(Set<String> methods) {
        for (String methodName : methods) {
            if (methodName.startsWith(UrlConstants.HTTPS_URL_PREFIX)
                    || methodName.startsWith(UrlConstants.HTTP_URL_PREFIX)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @return Whether the browser payment sheet should be skipped directly into the payment app.
     */
    private boolean shouldSkipAppSelector() {
        assert mBrowserPaymentRequest != null;
        assert mSpec != null;
        assert !mSpec.isDestroyed();

        PaymentApp selectedApp = mBrowserPaymentRequest.getSelectedPaymentApp();
        List<PaymentApp> allApps = mBrowserPaymentRequest.getPaymentApps();
        // If there is only a single payment app which can provide all merchant requested
        // information, we can safely go directly to the payment app instead of showing Payment
        // Request UI.
        return PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP)
                && allApps.size() >= 1
                && onlySingleAppCanProvideAllRequiredInformation(mSpec.getPaymentOptions(), allApps)
                // Skip to payment app only if it can be pre-selected.
                && selectedApp != null;
    }

    // Implements PaymentDetailsConverter.MethodChecker:
    @Override
    public boolean isInvokedInstrumentValidForPaymentMethodIdentifier(
            String methodName, PaymentApp invokedPaymentApp) {
        return invokedPaymentApp != null
                && invokedPaymentApp.isValidForPaymentMethodData(methodName, null);
    }

    private boolean isPaymentDetailsUpdateValid(PaymentDetails details) {
        // ID cannot be updated. Updating the total is optional.
        return details.id == null
                && mDelegate.validatePaymentDetails(details)
                && mBrowserPaymentRequest.parseAndValidateDetailsFurtherIfNeeded(details);
    }

    private String continueShowWithUpdatedDetails(@Nullable PaymentDetails details) {
        assert mIsShowWaitingForUpdatedDetails;
        assert mBrowserPaymentRequest != null;
        // mSpec.updateWith() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (details == null || !isPaymentDetailsUpdateValid(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            return ErrorStrings.INVALID_PAYMENT_DETAILS;
        }

        if (!TextUtils.isEmpty(details.error)) return ErrorStrings.INVALID_STATE;

        mSpec.updateWith(details);

        mIsShowWaitingForUpdatedDetails = false;
        String error =
                mBrowserPaymentRequest.continueShowWithUpdatedDetails(
                        mSpec.getPaymentDetails(), mIsFinishedQueryingPaymentApps);
        if (error != null) return error;

        if (!mIsFinishedQueryingPaymentApps) return null;
        return onShowCalledAndAppsQueriedAndDetailsFinalized();
    }

    /**
     * The component part of the {@link PaymentRequest#updateWith} implementation.
     *
     * @param details The details that the merchant provides to update the payment request, can be
     *     null.
     */
    /* package */ void updateWith(@Nullable PaymentDetails details) {
        if (mBrowserPaymentRequest == null) return;
        if (mIsShowWaitingForUpdatedDetails) {
            // Under this condition, updateWith() is called in response to the resolution of
            // show()'s PaymentDetailsUpdate promise.
            String error = continueShowWithUpdatedDetails(details);
            if (error != null) {
                onShowFailed(error);
                return;
            }
            return;
        }

        if (!mIsShowCalled) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW, PaymentErrorReason.USER_CANCEL);
            return;
        }

        boolean hasNotifiedInvokedPaymentApp =
                mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate();
        if (!PaymentOptionsUtils.requestAnyInformation(mPaymentOptions)
                && !hasNotifiedInvokedPaymentApp) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_STATE, PaymentErrorReason.USER_CANCEL);
            return;
        }

        if (details == null || !isPaymentDetailsUpdateValid(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS,
                    PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return;
        }
        mSpec.updateWith(details);

        if (hasNotifiedInvokedPaymentApp) {
            // After a payment app has been invoked, all of the merchant's calls to update the price
            // via updateWith() should be forwarded to the invoked app, so it can reflect the
            // updated price in its UI.
            mInvokedPaymentApp.updateWith(
                    PaymentDetailsConverter.convertToPaymentRequestDetailsUpdate(
                            details, /* methodChecker= */ this, mInvokedPaymentApp));
        }
        mBrowserPaymentRequest.onPaymentDetailsUpdated(
                mSpec.getPaymentDetails(), hasNotifiedInvokedPaymentApp);
    }

    /**
     * The component part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     */
    /* package */ void onPaymentDetailsNotUpdated() {
        if (mBrowserPaymentRequest == null) return;
        if (!mIsShowCalled) {
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
            mInvokedPaymentApp.abortPaymentApp(/* callback= */ this);
            return;
        }
        onInstrumentAbortResult(true);
    }

    /**
     * Completes the payment request. This method is triggered by PaymentResponse.complete() from
     * the renderer, used to notify the UI of the completion, closes the UI and opened resources and
     * close the payment request service.
     *
     * @param result The status of the transaction, defined in {@link PaymentComplete}, specified by
     *     the merchant with complete(result).
     */
    /* package */ void complete(int result) {
        if (mBrowserPaymentRequest == null) return;
        if (result != PaymentComplete.FAIL) {
            mJourneyLogger.setCompleted();
            assert mSpec.getRawTotal() != null;
        }

        mBrowserPaymentRequest.complete(result, this::onCompleteHandled);
    }

    private void onCompleteHandled() {
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onCompleteHandled();
        }
        if (sObserverForTest != null) {
            sObserverForTest.onCompletedHandled();
        }
        if (mClient != null) mClient.onComplete();
    }

    /**
     * The component part of the {@link PaymentRequest#retry} implementation. Check {@link
     * PaymentRequest#retry} for the parameters' specification.
     */
    /* package */ void retry(PaymentValidationErrors errors) {
        if (mBrowserPaymentRequest == null) return;
        if (!PaymentValidator.validatePaymentValidationErrors(errors)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_VALIDATION_ERRORS, PaymentErrorReason.USER_CANCEL);
            return;
        }
        assert mSpec != null;
        assert !mSpec.isDestroyed() : "mSpec should not be used after being destroyed.";
        mSpec.retry(errors);
        mBrowserPaymentRequest.onRetry(errors);
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
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

    /** The component part of the {@link PaymentRequest#hasEnrolledInstrument} implementation. */
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
     * closing triggered by other classes should call {@link #close} instead. The caller should stop
     * referencing this class after calling this method.
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
     * Called when the mojo connection with the renderer PaymentRequest has an error. The caller
     * should stop referencing this class after calling this method.
     *
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
     *
     * @param debugMessage The debug message to be sent to the renderer.
     */
    /* package */ void abortForInvalidDataFromRenderer(String debugMessage) {
        if (mJourneyLogger != null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
        }
        disconnectFromClientWithDebugMessage(
                debugMessage, PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
    }

    /**
     * Close this instance and release all of the retained resources. The external callers of this
     * method should stop referencing this instance upon calling. This method can be called within
     * itself without causing infinite loops.
     */
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        sShowingPaymentRequest = null;

        if (mSpcAuthnUiController != null) {
            mSpcAuthnUiController.hide();
            mSpcAuthnUiController = null;
        }

        if (mNoMatchingController != null) {
            mNoMatchingController.close();
            mNoMatchingController = null;
        }

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

        if (mJourneyLogger != null) {
            mJourneyLogger.destroy();
        }

        if (mSpec != null) {
            mSpec.destroy();
        }

        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onClosed();
        }

        PaymentDetailsUpdateServiceHelper.getInstance().reset();
    }

    /**
     * @return An observer for the payment request service, if any; otherwise, null.
     */
    @Nullable
    public static PaymentRequestServiceObserverForTest getObserverForTest() {
        return sObserverForTest;
    }

    /** Set an observer for the payment request service, cannot be null. */
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        assert observerForTest != null;
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
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

    /** Invokes {@link PaymentRequestClient.warnNoFavicon}. */
    public void warnNoFavicon() {
        if (mClient != null) mClient.warnNoFavicon();
    }

    /**
     * @return The logger of the user journey of the Android PaymentRequest service, cannot be null.
     */
    public JourneyLogger getJourneyLogger() {
        return mJourneyLogger;
    }

    /**
     * Redact shipping address before exposing it in ShippingAddressChangeEvent.
     * https://w3c.github.io/payment-request/#shipping-address-changed-algorithm
     *
     * @param shippingAddress The shipping address to redact in place.
     */
    private static void redactShippingAddress(PaymentAddress shippingAddress) {
        shippingAddress.organization = "";
        shippingAddress.phone = "";
        shippingAddress.recipient = "";
        shippingAddress.addressLine = new String[0];
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
        return true;
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

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    // Implements PaymentRequestUpdateEventListener:
    @Override
    public boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails) {
        if (TextUtils.isEmpty(methodName)
                || stringifiedDetails == null
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()
                || mClient == null) {
            return false;
        }
        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
        return true;
    }

    // Implements PaymentRequestUpdateEventListener:
    @Override
    public boolean changeShippingOptionFromInvokedApp(String shippingOptionId) {
        if (TextUtils.isEmpty(shippingOptionId)
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()
                || !mRequestShipping
                || mSpec.getRawShippingOptions() == null
                || mClient == null) {
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
        if (shippingAddress == null
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()
                || !mRequestShipping
                || mClient == null) {
            return false;
        }

        onShippingAddressChange(shippingAddress);
        return true;
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    @Override
    public void onInstrumentDetailsReady(
            String methodName, String stringifiedDetails, PayerData payerData) {
        assert methodName != null;
        assert stringifiedDetails != null;
        if (mPaymentResponseHelper == null || mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onInstrumentDetailsReady();
        mPaymentResponseHelper.generatePaymentResponse(
                methodName, stringifiedDetails, payerData, /* resultCallback= */ this);
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
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
        if (sNativeObserverForTest != null) sNativeObserverForTest.onErrorDisplayed();
        if (mBrowserPaymentRequest == null) return;
        if (mBrowserPaymentRequest.hasSkippedAppSelector()) {
            assert !TextUtils.isEmpty(errorMessage);
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(errorMessage, PaymentErrorReason.USER_CANCEL);
        } else {
            mBrowserPaymentRequest.showAppSelectorAfterPaymentAppInvokeFailed();
        }
    }

    @Nullable
    public static SecurePaymentConfirmationAuthnController
            getSecurePaymentConfirmationAuthnUiForTesting() {
        return sShowingPaymentRequest == null ? null : sShowingPaymentRequest.mSpcAuthnUiController;
    }

    @Nullable
    public static SecurePaymentConfirmationNoMatchingCredController
            getSecurePaymentConfirmationNoMatchingCredUiForTesting() {
        return sShowingPaymentRequest == null ? null : sShowingPaymentRequest.mNoMatchingController;
    }
}
