// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.payments.intent.IsReadyToPayServiceHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType;
import org.chromium.components.payments.intent.WebPaymentIntentHelperTypeConverter;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * The point of interaction with a locally installed 3rd party native Android payment app.
 * https://web.dev/articles/android-payment-apps-developers-guide
 */
@NullMarked
public class AndroidPaymentApp extends PaymentApp
        implements IsReadyToPayServiceHelper.ResultHandler, WindowAndroid.IntentCallback {
    private final Handler mHandler;
    private final AndroidIntentLauncher mLauncher;
    private final @Nullable DialogController mDialogController;
    private final Set<String> mMethodNames;
    private final boolean mIsIncognito;
    private final String mPackageName;
    private final String mPayActivityName;
    private final @Nullable String mIsReadyToPayServiceName;
    private final @Nullable String mPaymentDetailsUpdateServiceName;
    private final SupportedDelegations mSupportedDelegations;
    private final boolean mShowReadyToPayDebugInfo;
    private final boolean mRemoveDeprecatedFields;

    private @Nullable IsReadyToPayCallback mIsReadyToPayCallback;
    private @Nullable InstrumentDetailsCallback mInstrumentDetailsCallback;
    private @Nullable IsReadyToPayServiceHelper mIsReadyToPayServiceHelper;
    private @Nullable PaymentDetailsUpdateConnection mPaymentDetailsUpdateConnection;
    private final @Nullable String mApplicationIdentifierToHide;
    private boolean mBypassIsReadyToPayServiceInTest;
    private boolean mIsPreferred;

    // Set inside launchPaymentApp and used to validate the received response.
    private WebPaymentIntentHelperType.@Nullable PaymentOptions mPaymentOptions;

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param launcher Helps launching the Android payment app.
     * @param dialogController Helps showing informational or warning dialogs.
     * @param packageName The name of the package of the payment app.
     * @param activity The name of the payment activity in the payment app.
     * @param isReadyToPayService The name of the service that can answer "is ready to pay" query,
     *     or null of none.
     * @param paymentDetailsUpdateServiceName The name of the payment app's service for dynamically
     *     updating the payment details (e.g., the total price) based on changes in user's payment
     *     method, shipping address, or shipping option.
     * @param label The UI label to use for the payment app.
     * @param icon The icon to use in UI for the payment app.
     * @param isIncognito Whether the user is in incognito mode.
     * @param appToHide The identifier of the application that this app can hide.
     * @param supportedDelegations Delegations which this app can support.
     * @param showReadyToPayDebugInfo Whether IS_READY_TO_PAY intent should be displayed in a debug
     *     dialog.
     * @param removeDeprecatedFields Whether intents should omit deprecated fields.
     */
    public AndroidPaymentApp(
            AndroidIntentLauncher launcher,
            @Nullable DialogController dialogController,
            String packageName,
            String activity,
            @Nullable String isReadyToPayService,
            @Nullable String paymentDetailsUpdateServiceName,
            String label,
            Drawable icon,
            boolean isIncognito,
            @Nullable String appToHide,
            SupportedDelegations supportedDelegations,
            boolean showReadyToPayDebugInfo,
            boolean removeDeprecatedFields) {
        super(packageName, label, null, icon);
        ThreadUtils.assertOnUiThread();
        mHandler = new Handler();
        mLauncher = launcher;
        mDialogController = dialogController;

        mPackageName = packageName;
        mPayActivityName = activity;
        mIsReadyToPayServiceName = isReadyToPayService;
        mPaymentDetailsUpdateServiceName = paymentDetailsUpdateServiceName;

        if (mIsReadyToPayServiceName != null) {
            assert !isIncognito;
        }

        mMethodNames = new HashSet<>();
        mIsIncognito = isIncognito;
        mApplicationIdentifierToHide = appToHide;
        mSupportedDelegations = supportedDelegations;
        mShowReadyToPayDebugInfo = showReadyToPayDebugInfo;
        mRemoveDeprecatedFields = removeDeprecatedFields;
        mIsPreferred = false;
    }

    /** @param methodName A payment method that this app supports, e.g., "https://bobpay.com". */
    public void addMethodName(String methodName) {
        mMethodNames.add(methodName);
    }

    /** Callback for receiving responses to IS_READY_TO_PAY queries. */
    public interface IsReadyToPayCallback {
        /**
         * Called after it is known whether the given app is ready to pay.
         * @param app          The app that has been queried.
         * @param isReadyToPay Whether the app is ready to pay.
         */
        void onIsReadyToPayResponse(AndroidPaymentApp app, boolean isReadyToPay);
    }

    private static String buildReadyToPayDebugInfoString(
            String serviceName,
            String packageName,
            String origin,
            String iframeOrigin,
            Map<String, PaymentMethodData> methodDataMap) {
        StringBuilder sb = new StringBuilder();
        sb.append("IS_READY_TO_PAY sent to ");
        sb.append(serviceName);
        sb.append(" in ");
        sb.append(packageName);
        sb.append(" with {\"topLevelOrigin\": \"");
        sb.append(origin);
        sb.append("\", \"paymentRequestOrigin\": \"");
        sb.append(iframeOrigin);
        sb.append("\", \"methodNames\": [");
        for (String methodName : methodDataMap.keySet()) {
            sb.append("\"");
            sb.append(methodName);
            sb.append("\"");
        }
        sb.append("], \"methodData\": [");
        for (Map.Entry<String, PaymentMethodData> entry : methodDataMap.entrySet()) {
            sb.append("{\"");
            sb.append(entry.getKey());
            sb.append("\": ");
            sb.append(entry.getValue().stringifiedData);
            sb.append("}");
        }
        sb.append("]}");
        return sb.toString();
    }

    /** Queries the IS_READY_TO_PAY service. */
    public void maybeQueryIsReadyToPayService(
            Map<String, PaymentMethodData> methodDataMap,
            String origin,
            String iframeOrigin,
            byte @Nullable [][] certificateChain,
            Map<String, PaymentDetailsModifier> modifiers,
            IsReadyToPayCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mIsReadyToPayCallback == null
                : "Have not responded to previous IS_READY_TO_PAY request";

        mIsReadyToPayCallback = callback;
        if (mIsReadyToPayServiceName == null) {
            respondToIsReadyToPayQuery(true);
            return;
        }

        assert !mIsIncognito;

        if (mShowReadyToPayDebugInfo && mDialogController != null) {
            mDialogController.showReadyToPayDebugInfo(
                    buildReadyToPayDebugInfoString(
                            mIsReadyToPayServiceName,
                            mPackageName,
                            origin,
                            iframeOrigin,
                            methodDataMap));
        }

        Intent isReadyToPayIntent =
                WebPaymentIntentHelper.createIsReadyToPayIntent(
                        /* callerPackageName= */ ContextUtils.getApplicationContext()
                                .getPackageName(),
                        /* paymentAppPackageName= */ mPackageName,
                        /* paymentAppServiceName= */ mIsReadyToPayServiceName,
                        removeUrlScheme(origin),
                        removeUrlScheme(iframeOrigin),
                        certificateChain,
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(
                                methodDataMap),
                        // TODO(crbug.com/40212375): Re-enable clearing of identity for
                        // IS_READY_TO_PAY
                        /* clearIdFields= */ false,
                        mRemoveDeprecatedFields);
        if (mBypassIsReadyToPayServiceInTest) {
            respondToIsReadyToPayQuery(true);
            return;
        }
        mIsReadyToPayServiceHelper =
                new IsReadyToPayServiceHelper(
                        ContextUtils.getApplicationContext(),
                        isReadyToPayIntent,
                        /* resultHandler= */ this);
        mIsReadyToPayServiceHelper.query();
    }

    @VisibleForTesting
    public void bypassIsReadyToPayServiceInTest() {
        mBypassIsReadyToPayServiceInTest = true;
    }

    private void respondToIsReadyToPayQuery(boolean isReadyToPay) {
        ThreadUtils.assertOnUiThread();
        if (mIsReadyToPayCallback == null) return;
        mIsReadyToPayCallback.onIsReadyToPayResponse(/* app= */ this, isReadyToPay);
        mIsReadyToPayCallback = null;
    }

    @Override
    public @Nullable String getApplicationIdentifierToHide() {
        return mApplicationIdentifierToHide;
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public void invokePaymentApp(
            final String id,
            final String merchantName,
            String origin,
            String iframeOrigin,
            final byte @Nullable [][] certificateChain,
            final Map<String, PaymentMethodData> methodDataMap,
            final PaymentItem total,
            final List<PaymentItem> displayItems,
            final Map<String, PaymentDetailsModifier> modifiers,
            final PaymentOptions paymentOptions,
            final List<PaymentShippingOption> shippingOptions,
            InstrumentDetailsCallback callback) {
        mInstrumentDetailsCallback = callback;

        String schemelessOrigin = removeUrlScheme(origin);
        String schemelessIframeOrigin = removeUrlScheme(iframeOrigin);
        Runnable launchRunnable =
                () ->
                        launchPaymentApp(
                                id,
                                merchantName,
                                schemelessOrigin,
                                schemelessIframeOrigin,
                                certificateChain,
                                methodDataMap,
                                total,
                                displayItems,
                                modifiers,
                                paymentOptions,
                                shippingOptions);
        if (!mIsIncognito) {
            launchRunnable.run();
            return;
        }

        // The dialog controller can be null only in WebView, which does not have a concept of
        // incognito mode, i.e., `mIsIncognito` is always false in WebView.
        assert mDialogController != null;

        mDialogController.showLeavingIncognitoWarning(
                this::notifyErrorInvokingPaymentApp, launchRunnable);
    }

    @Override
    public void updateWith(PaymentRequestDetailsUpdate response) {
        ThreadUtils.assertOnUiThread();
        PaymentDetailsUpdateServiceHelper.getInstance()
                .updateWith(
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentRequestDetailsUpdate(
                                response));
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        ThreadUtils.assertOnUiThread();
        PaymentDetailsUpdateServiceHelper.getInstance().onPaymentDetailsNotUpdated();
    }

    @Override
    public boolean isWaitingForPaymentDetailsUpdate() {
        ThreadUtils.assertOnUiThread();
        return PaymentDetailsUpdateServiceHelper.getInstance().isWaitingForPaymentDetailsUpdate();
    }

    @Override
    public boolean handlesShippingAddress() {
        return mSupportedDelegations.getShippingAddress();
    }

    @Override
    public boolean handlesPayerName() {
        return mSupportedDelegations.getPayerName();
    }

    @Override
    public boolean handlesPayerEmail() {
        return mSupportedDelegations.getPayerEmail();
    }

    @Override
    public boolean handlesPayerPhone() {
        return mSupportedDelegations.getPayerPhone();
    }

    private static String removeUrlScheme(String url) {
        return UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }

    private void launchPaymentApp(
            String id,
            String merchantName,
            String origin,
            String iframeOrigin,
            byte @Nullable [][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap,
            PaymentItem total,
            List<PaymentItem> displayItems,
            Map<String, PaymentDetailsModifier> modifiers,
            PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions) {
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mInstrumentDetailsCallback != null;
        mPaymentOptions =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentOptions(paymentOptions);

        Intent payIntent =
                WebPaymentIntentHelper.createPayIntent(
                        mPackageName,
                        mPayActivityName,
                        id,
                        merchantName,
                        origin,
                        iframeOrigin,
                        certificateChain,
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(
                                methodDataMap),
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentItem(total),
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentItems(displayItems),
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentDetailsModifierMap(
                                modifiers),
                        mPaymentOptions,
                        WebPaymentIntentHelperTypeConverter.fromMojoShippingOptionList(
                                shippingOptions),
                        mRemoveDeprecatedFields);

        mLauncher.launchPaymentApp(
                payIntent, this::notifyErrorInvokingPaymentApp, /* intentCallback= */ this);

        if (!TextUtils.isEmpty(mPaymentDetailsUpdateServiceName)) {
            mPaymentDetailsUpdateConnection =
                    new PaymentDetailsUpdateConnection(
                            ContextUtils.getApplicationContext(),
                            WebPaymentIntentHelper.createPaymentDetailsUpdateServiceIntent(
                                    /* callerPackageName= */ ContextUtils.getApplicationContext()
                                            .getPackageName(),
                                    mPackageName,
                                    mPaymentDetailsUpdateServiceName),
                            new PaymentDetailsUpdateService().getBinder());
            mPaymentDetailsUpdateConnection.connectToService();
        }
    }

    private void notifyErrorInvokingPaymentApp(String errorMessage) {
        assert mInstrumentDetailsCallback != null : "Callback should be invoked only once";
        mHandler.post(
                () -> {
                    assert mInstrumentDetailsCallback != null
                            : "Callback should be invoked only once";
                    mInstrumentDetailsCallback.onInstrumentDetailsError(errorMessage);
                    mInstrumentDetailsCallback = null;
                });
    }

    // WindowAndroid.IntentCallback:
    @Override
    public void onIntentCompleted(int resultCode, Intent data) {
        assert mInstrumentDetailsCallback != null;
        ThreadUtils.assertOnUiThread();
        if (mPaymentDetailsUpdateConnection != null) {
            mPaymentDetailsUpdateConnection.terminateConnection();
        }
        WebPaymentIntentHelper.parsePaymentResponse(
                resultCode,
                data,
                mPaymentOptions,
                this::notifyErrorInvokingPaymentApp,
                this::onPaymentSuccess);
    }

    private void onPaymentSuccess(String methodName, String details, PayerData payerData) {
        assert mInstrumentDetailsCallback != null : "Callback should be invoked only once";
        mInstrumentDetailsCallback.onInstrumentDetailsReady(methodName, details, payerData);
        mInstrumentDetailsCallback = null;
    }

    @Override
    public void dismissInstrument() {}

    // IsReadyToPayServiceHelper.ResultHandler:
    @Override
    public void onIsReadyToPayServiceError() {
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> respondToIsReadyToPayQuery(false));
    }

    @Override
    public void onIsReadyToPayServiceResponse(boolean isReadyToPay) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> respondToIsReadyToPayQuery(isReadyToPay));
    }

    @Override
    public @PaymentAppType int getPaymentAppType() {
        return PaymentAppType.NATIVE_MOBILE_APP;
    }

    @Override
    public boolean isPreferred() {
        return mIsPreferred;
    }

    public void setIsPreferred(boolean isPreferred) {
        mIsPreferred = isPreferred;
    }

    /** @return The package name of the invoked native app. */
    public String packageName() {
        return mPackageName;
    }
}
