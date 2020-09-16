// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.Origin;

import java.util.List;

/**
 * {@link ComponentPaymentRequestImpl}, {@link MojoPaymentRequestGateKeeper} and PaymentRequestImpl
 * together make up the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This class provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.PaymentRequestImpl.
 * TODO(crbug.com/1102522): PaymentRequestImpl is under refactoring, with the purpose of moving the
 * business logic of PaymentRequestImpl into ComponentPaymentRequestImpl and eventually moving
 * PaymentRequestImpl. Note that the callers of the instances of this class need to close them with
 * {@link ComponentPaymentRequestImpl#close()}, after which no usage is allowed.
 */
public class ComponentPaymentRequestImpl {
    private static final String TAG = "CompPaymentRequest";
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
    private boolean mSkipUiForNonUrlPaymentMethodIdentifiers;
    private PaymentRequestLifecycleObserver mPaymentRequestLifecycleObserver;
    private boolean mHasClosed;

    // mClient is null only when it has closed.
    private PaymentRequestClient mClient;

    // mBrowserPaymentRequest is null when it has closed or is uninitiated.
    private BrowserPaymentRequest mBrowserPaymentRequest;

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
         * @return True if the merchant's web contents that initiated the payment request is active.
         */
        boolean isWebContentsActive();

        /**
         * @return Whether the preferences allow CAN_MAKE_PAYMENT.
         */
        boolean prefsCanMakePayment();

        /**
         * @return True if the UI can be skipped for "basic-card" scenarios. This will only ever be
         *         true in tests.
         */
        boolean skipUiForBasicCard();

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
         * Called after an instance of {@link ComponentPaymentRequestImpl} has been created.
         *
         * @param componentPaymentRequest The newly created instance of ComponentPaymentRequestImpl.
         */
        void onPaymentRequestCreated(ComponentPaymentRequestImpl componentPaymentRequest);

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
     * @param skipUiForBasicCard True if the PaymentRequest UI should be skipped when the request
     *         only supports basic-card methods.
     * @param browserPaymentRequestFactory The factory that generates BrowserPaymentRequest.
     * @return The created instance.
     */
    public static PaymentRequest createPaymentRequest(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, boolean skipUiForBasicCard, Delegate delegate,
            BrowserPaymentRequest.Factory browserPaymentRequestFactory) {
        return new MojoPaymentRequestGateKeeper(
                (client, methodData, details, options, googlePayBridgeEligible, onClosedListener)
                        -> ComponentPaymentRequestImpl.createIfParamsValid(renderFrameHost,
                                isOffTheRecord, skipUiForBasicCard, browserPaymentRequestFactory,
                                client, methodData, details, options, googlePayBridgeEligible,
                                onClosedListener, delegate));
    }

    /**
     * @return An instance of {@link ComponentPaymentRequestImpl} only if the parameters are deemed
     *         valid; Otherwise, null.
     */
    @Nullable
    private static ComponentPaymentRequestImpl createIfParamsValid(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, boolean skipUiForBasicCard,
            BrowserPaymentRequest.Factory browserPaymentRequestFactory,
            @Nullable PaymentRequestClient client, @Nullable PaymentMethodData[] methodData,
            @Nullable PaymentDetails details, @Nullable PaymentOptions options,
            boolean googlePayBridgeEligible, Runnable onClosedListener, Delegate delegate) {
        assert renderFrameHost != null;
        assert browserPaymentRequestFactory != null;
        assert onClosedListener != null;

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

        ComponentPaymentRequestImpl instance =
                new ComponentPaymentRequestImpl(client, renderFrameHost, webContents, journeyLogger,
                        options, skipUiForBasicCard, isOffTheRecord, onClosedListener, delegate);
        instance.onCreated();
        boolean valid = instance.initAndValidate(
                browserPaymentRequestFactory, methodData, details, googlePayBridgeEligible);
        if (!valid) {
            instance.close();
            return null;
        }
        return instance;
    }

    private void onCreated() {
        if (sObserverForTest == null) return;
        sObserverForTest.onPaymentRequestCreated(this);
    }

    /** Abort the request, used before this class's instantiation. */
    private static void abortBeforeInstantiation(@Nullable PaymentRequestClient client,
            @Nullable JourneyLogger journeyLogger, String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (client != null) client.onError(reason, debugMessage);
        if (journeyLogger != null) journeyLogger.setAborted(reason);
        if (sNativeObserverForTest != null) sNativeObserverForTest.onConnectionTerminated();
    }

    private ComponentPaymentRequestImpl(PaymentRequestClient client,
            RenderFrameHost renderFrameHost, WebContents webContents, JourneyLogger journeyLogger,
            PaymentOptions options, boolean skipUiForBasicCard, boolean isOffTheRecord,
            Runnable onClosedListener, Delegate delegate) {
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

        mMerchantName = mWebContents.getTitle();
        mCertificateChain = CertificateChainHelper.getCertificateChain(mWebContents);
        mIsOffTheRecord = isOffTheRecord;
        mSkipUiForNonUrlPaymentMethodIdentifiers = skipUiForBasicCard;
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

    private boolean initAndValidate(BrowserPaymentRequest.Factory factory,
            PaymentMethodData[] methodData, PaymentDetails details,
            boolean googlePayBridgeEligible) {
        mBrowserPaymentRequest = factory.createBrowserPaymentRequest(this);
        mJourneyLogger.recordCheckoutStep(
                org.chromium.components.payments.CheckoutFunnelStep.INITIATED);

        if (!UrlUtil.isOriginAllowedToUseWebPaymentApis(mWebContents.getLastCommittedUrl())) {
            Log.d(TAG, org.chromium.components.payments.ErrorStrings.PROHIBITED_ORIGIN);
            Log.d(TAG,
                    org.chromium.components.payments.ErrorStrings
                            .PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            mJourneyLogger.setAborted(
                    org.chromium.components.payments.AbortReason.INVALID_DATA_FROM_RENDERER);
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
            Log.d(TAG,
                    org.chromium.components.payments.ErrorStrings
                            .PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            mJourneyLogger.setAborted(
                    org.chromium.components.payments.AbortReason.INVALID_DATA_FROM_RENDERER);
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(rejectShowErrorMessage,
                    PaymentErrorReason.NOT_SUPPORTED_FOR_INVALID_ORIGIN_OR_SSL);
            return false;
        }

        return mBrowserPaymentRequest.initAndValidate(methodData, details, googlePayBridgeEligible);
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
        // ComponentPaymentRequestImpl#create().
        if (mClient != null) mClient.close();
        mClient = null;

        mOnClosedListener.run();
    }

    /**
     * Register an observer for the PaymentRequest lifecycle.
     * @param paymentRequestLifecycleObserver The observer, cannot be null.
     */
    public void registerPaymentRequestLifecycleObserver(
            PaymentRequestLifecycleObserver paymentRequestLifecycleObserver) {
        assert paymentRequestLifecycleObserver != null;
        mPaymentRequestLifecycleObserver = paymentRequestLifecycleObserver;
    }

    /** @return The observer for the PaymentRequest lifecycle, can be null. */
    @Nullable
    public PaymentRequestLifecycleObserver getPaymentRequestLifecycleObserver() {
        return mPaymentRequestLifecycleObserver;
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

    /**
     * @return True when skip UI is available for non-url based payment method identifiers (e.g.
     * basic-card).
     */
    public boolean skipUiForNonUrlPaymentMethodIdentifiers() {
        return mSkipUiForNonUrlPaymentMethodIdentifiers;
    }

    @VisibleForTesting
    public void setSkipUiForNonUrlPaymentMethodIdentifiersForTest() {
        mSkipUiForNonUrlPaymentMethodIdentifiers = true;
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
