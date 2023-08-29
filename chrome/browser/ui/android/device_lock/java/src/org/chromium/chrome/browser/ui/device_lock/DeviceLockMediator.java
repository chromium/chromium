// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.accounts.Account;
import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountReauthenticationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator handles which design the device lock UI displays and interacts through the
 * coordinator delegate.
 */
public class DeviceLockMediator {
    private final PropertyModel mModel;
    private final DeviceLockCoordinator.Delegate mDelegate;

    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final @Nullable Account mAccount;
    private final ReauthenticatorBridge mDeviceLockAuthenticatorBridge;
    private final AccountReauthenticationUtils mAccountReauthenticationUtils;

    public DeviceLockMediator(DeviceLockCoordinator.Delegate delegate, WindowAndroid windowAndroid,
            ReauthenticatorBridge deviceLockAuthenticatorBridge, Activity activity,
            @Nullable Account account) {
        this(delegate, windowAndroid, deviceLockAuthenticatorBridge,
                new AccountReauthenticationUtils(), activity, account);
    }

    protected DeviceLockMediator(DeviceLockCoordinator.Delegate delegate,
            WindowAndroid windowAndroid, ReauthenticatorBridge deviceLockAuthenticatorBridge,
            AccountReauthenticationUtils accountReauthenticationUtils, Activity activity,
            @Nullable Account account) {
        mDelegate = delegate;
        mActivity = activity;
        mAccount = account;
        mWindowAndroid = windowAndroid;
        mDeviceLockAuthenticatorBridge = deviceLockAuthenticatorBridge;
        mAccountReauthenticationUtils = accountReauthenticationUtils;
        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(PREEXISTING_DEVICE_LOCK, isDeviceLockPresent())
                         .with(DEVICE_SUPPORTS_PIN_CREATION_INTENT,
                                 DeviceLockUtils.isDeviceLockCreationIntentSupported(mActivity))
                         .with(IN_SIGN_IN_FLOW, account != null)
                         .with(ON_CREATE_DEVICE_LOCK_CLICKED, v -> onCreateDeviceLockClicked())
                         .with(ON_GO_TO_OS_SETTINGS_CLICKED, v -> onGoToOSSettingsClicked())
                         .with(ON_USER_UNDERSTANDS_CLICKED, v -> onUserUnderstandsClicked())
                         .with(ON_DISMISS_CLICKED, v -> delegate.onDeviceLockRefused())
                         .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    private AccountManagerFacade getAccountManager() {
        return AccountManagerFacadeProvider.getInstance();
    }

    private boolean isDeviceLockPresent() {
        KeyguardManager keyguardManager =
                (KeyguardManager) mActivity.getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager.isDeviceSecure();
    }

    private void onCreateDeviceLockClicked() {
        navigateToDeviceLockCreation(DeviceLockUtils.createDeviceLockDirectlyIntent(),
                () -> maybeTriggerAccountReauthenticationChallenge(mDelegate::onDeviceLockReady));
    }

    private void onGoToOSSettingsClicked() {
        navigateToDeviceLockCreation(DeviceLockUtils.createDeviceLockThroughOSSettingsIntent(),
                () -> maybeTriggerAccountReauthenticationChallenge(mDelegate::onDeviceLockReady));
    }

    private void onUserUnderstandsClicked() {
        triggerDeviceLockChallenge(
                () -> maybeTriggerAccountReauthenticationChallenge(mDelegate::onDeviceLockReady));
    }

    private void navigateToDeviceLockCreation(Intent intent, Runnable onSuccess) {
        if (isDeviceLockPresent()) {
            onSuccess.run();
            return;
        }
        mWindowAndroid.showIntent(intent, (resultCode, data) -> {
            if (isDeviceLockPresent()) {
                onSuccess.run();
            }
        }, null);
    }

    private void triggerDeviceLockChallenge(Runnable onSuccess) {
        mDeviceLockAuthenticatorBridge.reauthenticate((authSucceeded) -> {
            if (authSucceeded) {
                onSuccess.run();
            }
        }, false);
    }

    private void maybeTriggerAccountReauthenticationChallenge(Runnable onSuccess) {
        // If no account is specified, the current flow does not require account reauthentication.
        if (mAccount == null) {
            onSuccess.run();
            return;
        }
        mAccountReauthenticationUtils.confirmCredentialsOrRecentAuthentication(
                getAccountManager(), mAccount, mActivity, (confirmationResult) -> {
                    if (confirmationResult
                            == AccountReauthenticationUtils.ConfirmationResult.SUCCESS) {
                        onSuccess.run();
                    }
                });
    }
}
