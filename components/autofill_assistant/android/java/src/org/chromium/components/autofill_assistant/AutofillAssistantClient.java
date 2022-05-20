// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.accounts.Account;
import android.app.Activity;
import android.os.Build;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.autofill_assistant.user_data.GmsIntegrator;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content.browser.accessibility.BrowserAccessibilityState;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * An Autofill Assistant client, associated with a specific WebContents.
 *
 * This mainly a bridge to autofill_assistant::ClientAndroid.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantClient {
    /**
     * Pointer to the corresponding native autofill_assistant::ClientAndroid instance. Might be 0 if
     * the native instance has been deleted. Always check before use.
     */
    private long mNativeClientAndroid;

    /**
     * Util for fetching access tokens.
     */
    private final AssistantAccessTokenUtil mAccessTokenUtil;

    /**
     * Indicates whether account initialization was started.
     */
    private boolean mAccountInitializationStarted;

    /**
     * Indicates whether {@link mAccount} has been initialized.
     */
    private boolean mAccountInitialized;

    /** If set, fetch the access token once the accounts are fetched. */
    private boolean mShouldFetchAccessToken;

    /** If set, fetch the payments client token once the accounts are fetched. */
    private boolean mShouldFetchPaymentsClientToken;

    /** Returns the client for the given web contents, null if it does not exist. */
    public static AutofillAssistantClient fromWebContents(WebContents webContents) {
        return AutofillAssistantClientJni.get().fromWebContents(webContents);
    }

    /** Returns the client for the given web contents, creating it if necessary. */
    public static @Nullable AutofillAssistantClient createForWebContents(
            WebContents webContents, AssistantDependencies dependencies) {
        return AutofillAssistantClientJni.get().createForWebContents(webContents, dependencies);
    }

    /**
     * Notifies that an onboarding UI is shown or hidden.
     */
    public static void onOnboardingUiChange(WebContents webContents, boolean shown) {
        AutofillAssistantClientJni.get().onOnboardingUiChange(webContents, shown);
    }

    @CalledByNative
    private AutofillAssistantClient(
            long nativeClientAndroid, AssistantAccessTokenUtil accessTokenUtil) {
        mNativeClientAndroid = nativeClientAndroid;
        mAccessTokenUtil = accessTokenUtil;

        // Add listener for accessibility services with "FEEDBACK_SPOKEN" feedback type.
        BrowserAccessibilityState.Listener listener = (unused) -> {
            if (mNativeClientAndroid == 0) return;

            AutofillAssistantClientJni.get().onSpokenFeedbackAccessibilityServiceChanged(
                    mNativeClientAndroid, AutofillAssistantClient.this,
                    isSpokenFeedbackAccessibilityServiceEnabled());
        };
        // BrowserAccessibilityState listeners are garbage-collected and automatically removed
        // from the set of active listeners.
        BrowserAccessibilityState.addListener(listener);
    }

    /**
     * Gets rid of the UI, if there is one. Leaves Autofill Assistant running.
     */
    public void destroyUi() {
        if (mNativeClientAndroid == 0) return;

        AutofillAssistantClientJni.get().onJavaDestroyUI(
                mNativeClientAndroid, AutofillAssistantClient.this);
    }

    /**
     * Transfers ownership of the UI to an instance of Autofill Assistant running on
     * the given tab/WebContents. Leaves Autofill Assistant running.
     *
     * <p>If Autofill Assistant is not running on the given WebContents, the UI is destroyed, as if
     * {@link #destroyUi} was called.
     */
    public void transferUiTo(WebContents otherWebContents) {
        if (mNativeClientAndroid == 0) return;

        AutofillAssistantClientJni.get().transferUITo(
                mNativeClientAndroid, AutofillAssistantClient.this, otherWebContents);
    }

    /** Starts the controller and fetching scripts for websites. */
    public void fetchWebsiteActions(String userName, String experimentIds,
            Map<String, String> arguments, Callback<Boolean> callback) {
        if (mNativeClientAndroid == 0) {
            callback.onResult(false);
            return;
        }

        chooseAccountAsyncIfNecessary(userName.isEmpty() ? null : userName);

        // The native side calls sendDirectActionList() on the callback once the controller has
        // results.
        AutofillAssistantClientJni.get().fetchWebsiteActions(mNativeClientAndroid,
                AutofillAssistantClient.this, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), callback);
    }

    /** Return true if the controller exists and is in tracking mode. */
    public boolean hasRunFirstCheck() {
        if (mNativeClientAndroid == 0) {
            return false;
        }

        ThreadUtils.assertOnUiThread();
        return AutofillAssistantClientJni.get().hasRunFirstCheck(
                mNativeClientAndroid, AutofillAssistantClient.this);
    }

    /** Lists available direct actions. */
    public List<AutofillAssistantDirectAction> getDirectActions() {
        if (mNativeClientAndroid == 0) {
            return Collections.emptyList();
        }
        AutofillAssistantDirectAction[] actions = AutofillAssistantClientJni.get().getDirectActions(
                mNativeClientAndroid, AutofillAssistantClient.this);
        return Arrays.asList(actions);
    }

    /**
     * Performs a direct action.
     *
     * @param actionId id of the action
     * @param experimentIds comma-separated set of experiments to use while running the flow
     * @param arguments report these as script parameters while performing this specific action
     * @param overlayCoordinator if non-null, reuse existing UI elements, usually created to show
     *         onboarding.
     * @return true if the action was found started, false otherwise. The action can still fail
     * after this method returns true; the failure will be displayed on the UI.
     */
    public boolean performDirectAction(String actionId, String experimentIds,
            Map<String, String> arguments,
            @Nullable AssistantOverlayCoordinator overlayCoordinator) {
        if (mNativeClientAndroid == 0) return false;

        // Note that only fetchWebsiteActions can start AA, so only it needs
        // chooseAccountAsyncIfNecessary.
        return AutofillAssistantClientJni.get().performDirectAction(mNativeClientAndroid,
                AutofillAssistantClient.this, actionId, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), overlayCoordinator);
    }

    /**
     * Displays a generic error message. Intended for direct actions only, to let specific direct
     * actions show an error on failure.
     */
    public void showFatalError() {
        if (mNativeClientAndroid == 0) return;
        AutofillAssistantClientJni.get().showFatalError(
                mNativeClientAndroid, AutofillAssistantClient.this);
    }

    @CalledByNative
    private void chooseAccountAsyncIfNecessary(@Nullable String userName) {
        if (mAccountInitializationStarted) return;
        mAccountInitializationStarted = true;
    }

    @CalledByNative
    private void fetchAccessToken() {
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchAccessToken = true;
            return;
        }
        if (mNativeClientAndroid != 0) {
            AutofillAssistantClientJni.get().onAccessToken(
                    mNativeClientAndroid, AutofillAssistantClient.this, true, "");
        }
    }

    @CalledByNative
    private void invalidateAccessToken(String accessToken) {

    }

    /** Returns the e-mail address that corresponds to the access token or an empty string. */
    @CalledByNative
    private String getEmailAddressForAccessTokenAccount() {
        return "";
    }

    @CalledByNative
    private void fetchPaymentsClientToken() {
        if (mNativeClientAndroid == 0) {
            return;
        }
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchPaymentsClientToken = true;
            return;
        }
        byte[] emptyToken = new byte[0];
        // If there is no account, send an empty token.
        AutofillAssistantClientJni.get().onPaymentsClientToken(
                mNativeClientAndroid, AutofillAssistantClient.this, emptyToken);
    }

    /** Returns the android version of the device. */
    @CalledByNative
    private int getSdkInt() {
        return Build.VERSION.SDK_INT;
    }

    /** Returns the manufacturer of the device. */
    @CalledByNative
    private String getDeviceManufacturer() {
        return Build.MANUFACTURER;
    }

    /** Returns the model of the device. */
    @CalledByNative
    private String getDeviceModel() {
        return Build.MODEL;
    }

    /**
     * Returns whether an accessibility service with "FEEDBACK_SPOKEN" feedback type is enabled
     * or not.
     */
    @CalledByNative
    private boolean isSpokenFeedbackAccessibilityServiceEnabled() {
        return (BrowserAccessibilityState.getAccessibilityServiceFeedbackTypeMask()
                       & AccessibilityServiceInfo.FEEDBACK_SPOKEN)
                != 0;
    }

    /** Adds a dynamic action to the given reporter. */
    @CalledByNative
    private void onFetchWebsiteActions(Callback<Boolean> callback, boolean success) {
        callback.onResult(success);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeClientAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        AutofillAssistantClient fromWebContents(WebContents webContents);
        AutofillAssistantClient createForWebContents(
                WebContents webContents, AssistantDependencies dependencies);
        void onOnboardingUiChange(WebContents webContents, boolean shown);
        void onAccessToken(long nativeClientAndroid, AutofillAssistantClient caller,
                boolean success, String accessToken);
        void onPaymentsClientToken(
                long nativeClientAndroid, AutofillAssistantClient caller, byte[] clientToken);
        String getPrimaryAccountName(long nativeClientAndroid, AutofillAssistantClient caller);
        void onJavaDestroyUI(long nativeClientAndroid, AutofillAssistantClient caller);
        void transferUITo(
                long nativeClientAndroid, AutofillAssistantClient caller, Object otherWebContents);
        void fetchWebsiteActions(long nativeClientAndroid, AutofillAssistantClient caller,
                String experimentIds, String[] argumentNames, String[] argumentValues,
                Object callback);
        boolean hasRunFirstCheck(long nativeClientAndroid, AutofillAssistantClient caller);
        AutofillAssistantDirectAction[] getDirectActions(
                long nativeClientAndroid, AutofillAssistantClient caller);
        boolean performDirectAction(long nativeClientAndroid, AutofillAssistantClient caller,
                String actionId, String experimentId, String[] argumentNames,
                String[] argumentValues, @Nullable AssistantOverlayCoordinator overlayCoordinator);
        void showFatalError(long nativeClientAndroid, AutofillAssistantClient caller);
        void onSpokenFeedbackAccessibilityServiceChanged(
                long nativeClientAndroid, AutofillAssistantClient caller, boolean enabled);
        AssistantDependencies getDependencies(
                long nativeClientAndroid, AutofillAssistantClient caller);
    }
}
