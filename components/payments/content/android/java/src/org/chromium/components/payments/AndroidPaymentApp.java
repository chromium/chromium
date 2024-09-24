// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.payments.intent.IsReadyToPayServiceHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType;
import org.chromium.components.payments.intent.WebPaymentIntentHelperTypeConverter;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
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
public class AndroidPaymentApp extends PaymentApp
        implements IsReadyToPayServiceHelper.ResultHandler {
    private final Handler mHandler;
    private final Launcher mLauncher;
    private final Set<String> mMethodNames;
    private final boolean mIsIncognito;
    private final String mPackageName;
    private final String mPayActivityName;
    private final String mIsReadyToPayServiceName;
    private final SupportedDelegations mSupportedDelegations;
    private final boolean mShowReadyToPayDebugInfo;

    private IsReadyToPayCallback mIsReadyToPayCallback;
    private InstrumentDetailsCallback mInstrumentDetailsCallback;
    private IsReadyToPayServiceHelper mIsReadyToPayServiceHelper;
    @Nullable private String mApplicationIdentifierToHide;
    private boolean mBypassIsReadyToPayServiceInTest;
    private boolean mIsPreferred;

    // Set inside launchPaymentApp and used to validate the received response.
    @Nullable private WebPaymentIntentHelperType.PaymentOptions mPaymentOptions;

    /**
     * The interface for launching Android payment apps and for showing a warning about leaving
     * incognito mode when launching an Android payment app.
     */
    public interface Launcher {
        /**
         * Show an informational dialog about the contents of the given IS_READY_TO_PAY intent.
         *
         * @param readyToPayDebugInfo The informational message to display in a dialog for debugging
         *     purposes.
         */
        void showReadyToPayDebugInfo(String readyToPayDebugInfo);

        /**
         * Show a warning about leaving incognito mode with a prompt to continue into the payment
         * app.
         *
         * @param denyCallback The callback invoked when the user denies or dismisses the prompt.
         * @param approveCallback The callback invoked when the user approves the prompt.
         */
        default void showLeavingIncognitoWarning(
                Callback<String> denyCallback, Runnable approveCallback) {}

        /**
         * Launch the payment app via an intent.
         * @param intent The intent that includes the payment app identification and parameters.
         * @param errorCallback The callback invoked when invoking the payment app fails.
         * @param intentCallback The callback invoked when the payment app responds to the intent.
         */
        default void launchPaymentApp(
                Intent intent,
                Callback<String> errorCallback,
                Callback<IntentResult> intentCallback) {}
    }

    /** The result of invoking an Android app. */
    public static class IntentResult {
        /** Activity result, either Activity.RESULT_OK or Activity.RESULT_CANCELED. */
        public int resultCode;

        /** The data returned from the payment app. */
        public Intent data;
    }

    /**
     * The default implementation of payment app launcher that uses WindowAndroid for invoking
     * Android apps.
     */
    public static class LauncherImpl implements Launcher, WindowAndroid.IntentCallback {
        private final WebContents mWebContents;
        private Callback<IntentResult> mIntentCallback;

        /**
         * @param webContents The web contents whose WindowAndroid should be used for invoking
         * Android payment apps and receiving the result.
         */
        public LauncherImpl(WebContents webContents) {
            mWebContents = webContents;
        }

        @Nullable
        private Context getActivityContext() {
            WindowAndroid window = mWebContents.getTopLevelNativeWindow();
            return window == null ? null : window.getActivity().get();
        }

        @Override
        public void showReadyToPayDebugInfo(String readyToPayDebugInfo) {
            Context context = getActivityContext();
            if (context == null) {
                return;
            }
            new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                    .setMessage(readyToPayDebugInfo)
                    .show();
        }

        // Launcher implementation.
        @Override
        public void showLeavingIncognitoWarning(
                Callback<String> denyCallback, Runnable approveCallback) {
            Context context = getActivityContext();
            if (context == null) {
                denyCallback.onResult(ErrorStrings.ACTIVITY_NOT_FOUND);
                return;
            }
            new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                    .setTitle(R.string.external_app_leave_incognito_warning_title)
                    .setMessage(R.string.external_payment_app_leave_incognito_warning)
                    .setPositiveButton(
                            R.string.ok, (OnClickListener) (dialog, which) -> approveCallback.run())
                    .setNegativeButton(
                            R.string.cancel,
                            (OnClickListener)
                                    (dialog, which) ->
                                            denyCallback.onResult(ErrorStrings.USER_CANCELLED))
                    .setOnCancelListener(
                            dialog -> denyCallback.onResult(ErrorStrings.USER_CANCELLED))
                    .show();
        }

        // Launcher implementation.
        @Override
        public void launchPaymentApp(
                Intent intent,
                Callback<String> errorCallback,
                Callback<IntentResult> intentCallback) {
            assert mIntentCallback == null;

            if (mWebContents.isDestroyed()) {
                errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
                return;
            }

            WindowAndroid window = mWebContents.getTopLevelNativeWindow();
            if (window == null) {
                errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
                return;
            }

            mIntentCallback = intentCallback;
            try {
                if (!window.showIntent(
                        intent, /* callback= */ this, R.string.payments_android_app_error)) {
                    errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
                }
            } catch (SecurityException e) {
                // Payment app does not have android:exported="true" on the PAY activity.
                errorCallback.onResult(ErrorStrings.PAYMENT_APP_PRIVATE_ACTIVITY);
            }
        }

        // WindowAndroid.IntentCallback implementation.
        @Override
        public void onIntentCompleted(int resultCode, Intent data) {
            assert mIntentCallback != null;
            IntentResult intentResult = new IntentResult();
            intentResult.resultCode = resultCode;
            intentResult.data = data;
            mIntentCallback.onResult(intentResult);
            mIntentCallback = null;
        }
    }

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param launcher Helps querying and launching the Android payment app. Overridden in unit
     *     tests.
     * @param packageName The name of the package of the payment app.
     * @param activity The name of the payment activity in the payment app.
     * @param isReadyToPayService The name of the service that can answer "is ready to pay" query,
     *     or null of none.
     * @param label The UI label to use for the payment app.
     * @param icon The icon to use in UI for the payment app.
     * @param isIncognito Whether the user is in incognito mode.
     * @param appToHide The identifier of the application that this app can hide.
     * @param supportedDelegations Delegations which this app can support.
     * @param showReadyToPayDebugInfo Whether IS_READY_TO_PAY intent should be displayed in a debug
     *     dialog.
     */
    public AndroidPaymentApp(
            Launcher launcher,
            String packageName,
            String activity,
            @Nullable String isReadyToPayService,
            String label,
            Drawable icon,
            boolean isIncognito,
            @Nullable String appToHide,
            SupportedDelegations supportedDelegations,
            boolean showReadyToPayDebugInfo) {
        super(packageName, label, null, icon);
        ThreadUtils.assertOnUiThread();
        mHandler = new Handler();
        mLauncher = launcher;

        mPackageName = packageName;
        mPayActivityName = activity;
        mIsReadyToPayServiceName = isReadyToPayService;

        if (mIsReadyToPayServiceName != null) {
            assert !isIncognito;
        }

        mMethodNames = new HashSet<>();
        mIsIncognito = isIncognito;
        mApplicationIdentifierToHide = appToHide;
        mSupportedDelegations = supportedDelegations;
        mShowReadyToPayDebugInfo = showReadyToPayDebugInfo;
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
            @Nullable byte[][] certificateChain,
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

        if (mShowReadyToPayDebugInfo) {
            mLauncher.showReadyToPayDebugInfo(
                    buildReadyToPayDebugInfoString(
                            mIsReadyToPayServiceName,
                            mPackageName,
                            origin,
                            iframeOrigin,
                            methodDataMap));
        }

        Intent isReadyToPayIntent =
                WebPaymentIntentHelper.createIsReadyToPayIntent(
                        /* packageName= */ mPackageName,
                        /* serviceName= */ mIsReadyToPayServiceName,
                        removeUrlScheme(origin),
                        removeUrlScheme(iframeOrigin),
                        certificateChain,
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(
                                methodDataMap),
                        // TODO(crbug.com/40212375): Re-enable clearing of identity for
                        // IS_READY_TO_PAY
                        /* clearIdFields= */ false);
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
    @Nullable
    public String getApplicationIdentifierToHide() {
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
            final byte[][] certificateChain,
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

        mLauncher.showLeavingIncognitoWarning(this::notifyErrorInvokingPaymentApp, launchRunnable);
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
            byte[][] certificateChain,
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
                                shippingOptions));

        mLauncher.launchPaymentApp(
                payIntent, this::notifyErrorInvokingPaymentApp, this::onIntentCompleted);
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

    public void onIntentCompletedForTesting(IntentResult intentResult) {
        onIntentCompleted(intentResult);
    }

    private void onIntentCompleted(IntentResult intentResult) {
        assert mInstrumentDetailsCallback != null;
        ThreadUtils.assertOnUiThread();
        WebPaymentIntentHelper.parsePaymentResponse(
                intentResult.resultCode,
                intentResult.data,
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
