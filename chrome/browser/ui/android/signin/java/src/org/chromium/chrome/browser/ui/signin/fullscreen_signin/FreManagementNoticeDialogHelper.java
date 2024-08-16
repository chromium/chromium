// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.content.Context;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.ui.signin.ConfirmManagedSyncDataDialogCoordinator;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper for showing management notice dialog and record metrics during the fre sign-in flow. */
final class FreManagementNoticeDialogHelper {
    private static final String UNMANAGED_SIGNIN_DURATION_NAME =
            "Signin.Android.FREUnmanagedAccountSigninDuration";
    private static final String FRE_SIGNIN_EVENTS_NAME = "Signin.Android.FRESigninEvents";

    @IntDef({
        FRESigninEvents.CHECKING_MANAGED_STATUS,
        FRESigninEvents.SIGNING_IN_UNMANAGED,
        FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED,
        FRESigninEvents.ACCEPTING_MANAGEMENT,
        FRESigninEvents.SIGNING_IN_MANAGED,
        FRESigninEvents.SIGNIN_COMPLETE_MANAGED,
        FRESigninEvents.SIGNIN_ABORTED_UNMANAGED,
        FRESigninEvents.SIGNIN_ABORTED_MANAGED,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface FRESigninEvents {
        int CHECKING_MANAGED_STATUS = 0;
        int SIGNING_IN_UNMANAGED = 1;
        int SIGNIN_COMPLETE_UNMANAGED = 2;
        int ACCEPTING_MANAGEMENT = 3;
        int SIGNING_IN_MANAGED = 4;
        int SIGNIN_COMPLETE_MANAGED = 5;
        int SIGNIN_ABORTED_UNMANAGED = 6;
        int SIGNIN_ABORTED_MANAGED = 7;
        int NUM_ENTRIES = 8;
    }

    private FreManagementNoticeDialogHelper() {}

    private static class WrappedSigninCallback implements SigninManager.SignInCallback {
        private SigninManager.SignInCallback mWrappedCallback;

        public WrappedSigninCallback(SigninManager.SignInCallback callback) {
            mWrappedCallback = callback;
        }

        @Override
        public void onSignInComplete() {
            if (mWrappedCallback != null) mWrappedCallback.onSignInComplete();
        }

        @Override
        public void onPrefsCommitted() {
            if (mWrappedCallback != null) mWrappedCallback.onPrefsCommitted();
        }

        @Override
        public void onSignInAborted() {
            if (mWrappedCallback != null) mWrappedCallback.onSignInAborted();
        }
    }

    /**
     * Performs signin after confirming account management with the user, if necessary. Also records
     * FRE metrics.
     *
     * <p>TODO(crbug.com/349787455): Do not record FRE metrics in the upgrade promo.
     */
    static void checkAccountManagementAndSignIn(
            CoreAccountInfo coreAccountInfo,
            SigninManager signinManager,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback,
            Context context,
            ModalDialogManager modalDialogManager) {
        long startTimeMillis = SystemClock.uptimeMillis();
        if (signinManager.getUserAcceptedAccountManagement()) {
            SignInCallback wrappedCallback =
                    new WrappedSigninCallback(callback) {
                        @Override
                        public void onSignInComplete() {
                            RecordHistogram.recordMediumTimesHistogram(
                                    UNMANAGED_SIGNIN_DURATION_NAME,
                                    SystemClock.uptimeMillis() - startTimeMillis);
                            recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED);
                            super.onSignInComplete();
                        }

                        @Override
                        public void onSignInAborted() {
                            recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_UNMANAGED);
                            super.onSignInAborted();
                        }
                    };
            recordFREEvent(FRESigninEvents.SIGNING_IN_UNMANAGED);
            signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
            return;
        }
        recordFREEvent(FRESigninEvents.CHECKING_MANAGED_STATUS);
        signinManager.isAccountManaged(
                coreAccountInfo,
                (Boolean isAccountManaged) -> {
                    onIsAccountManaged(
                            isAccountManaged,
                            coreAccountInfo,
                            signinManager,
                            accessPoint,
                            callback,
                            context,
                            modalDialogManager,
                            startTimeMillis);
                });
    }

    private static void onIsAccountManaged(
            Boolean isAccountManaged,
            CoreAccountInfo coreAccountInfo,
            SigninManager signinManager,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback,
            Context context,
            ModalDialogManager modalDialogManager,
            long startTimeMillis) {
        if (!isAccountManaged) {
            SignInCallback wrappedCallback =
                    new WrappedSigninCallback(callback) {
                        @Override
                        public void onSignInComplete() {
                            RecordHistogram.recordMediumTimesHistogram(
                                    UNMANAGED_SIGNIN_DURATION_NAME,
                                    SystemClock.uptimeMillis() - startTimeMillis);
                            recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED);
                            super.onSignInComplete();
                        }

                        @Override
                        public void onSignInAborted() {
                            recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_UNMANAGED);
                            super.onSignInAborted();
                        }
                    };
            recordFREEvent(FRESigninEvents.SIGNING_IN_UNMANAGED);
            signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
            return;
        }

        SignInCallback wrappedCallback =
                new WrappedSigninCallback(callback) {
                    @Override
                    public void onSignInAborted() {
                        recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_MANAGED);
                        // If signin is aborted, we need to clear the account management acceptance.
                        signinManager.setUserAcceptedAccountManagement(false);
                        super.onSignInAborted();
                    }

                    @Override
                    public void onSignInComplete() {
                        recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_MANAGED);
                        super.onSignInComplete();
                    }
                };

        ConfirmManagedSyncDataDialogCoordinator.Listener listener =
                new ConfirmManagedSyncDataDialogCoordinator.Listener() {
                    @Override
                    public void onConfirm() {
                        signinManager.setUserAcceptedAccountManagement(true);
                        recordFREEvent(FRESigninEvents.SIGNING_IN_MANAGED);
                        signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
                    }

                    @Override
                    public void onCancel() {
                        if (callback != null) callback.onSignInAborted();
                    }
                };

        recordFREEvent(FRESigninEvents.ACCEPTING_MANAGEMENT);
        new ConfirmManagedSyncDataDialogCoordinator(
                context,
                modalDialogManager,
                listener,
                signinManager.extractDomainName(coreAccountInfo.getEmail()));
    }

    private static void recordFREEvent(@FRESigninEvents int event) {
        RecordHistogram.recordEnumeratedHistogram(
                FRE_SIGNIN_EVENTS_NAME, event, FRESigninEvents.NUM_ENTRIES);
    }
}
