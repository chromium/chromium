// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninTrackerBridge;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;

/**
 * Waits for a freshly signed-in account to be available in the Gaia cookies, and reports the
 * outcome as a {@link PostSigninOperationResult}. Errors are recorded with {@link
 * SigninMetricsUtils#logAccountConsistencyPromoAction} for the given access point, as callers show
 * an error screen for them.
 *
 * <p>This is meant to be used by sign-in flows that require the web session to be ready before
 * continuing, e.g. to load a Gaia page right after the sign-in.
 */
@MainThread
@NullMarked
public class SigninCookieWaiter {
    private final Profile mProfile;
    private final WebSigninTrackerBridge.Factory mWebSigninTrackerBridgeFactory;
    private final @SigninAccessPoint int mAccessPoint;
    // The bridge of the pending wait, if any.
    private @Nullable WebSigninTrackerBridge mWebSigninTrackerBridge;

    /**
     * @param profile The profile in which the sign-in happens.
     * @param webSigninTrackerBridgeFactory A {@link WebSigninTrackerBridge.Factory} to create
     *     {@link WebSigninTrackerBridge} instances.
     * @param accessPoint The sign-in access point, used for metrics.
     */
    public SigninCookieWaiter(
            Profile profile,
            WebSigninTrackerBridge.Factory webSigninTrackerBridgeFactory,
            @SigninAccessPoint int accessPoint) {
        mProfile = profile;
        mWebSigninTrackerBridgeFactory = webSigninTrackerBridgeFactory;
        mAccessPoint = accessPoint;
    }

    /**
     * Starts waiting for the account to be available in cookies. Cancels any pending wait, whose
     * callback will not be invoked.
     *
     * @param accountId The account that was just signed in.
     * @param onComplete Called on the UI thread with the result of the wait. May be called
     *     synchronously, e.g. if the account is already in the cookies. May start a new wait.
     */
    public void waitForCookies(
            CoreAccountId accountId, Callback<@PostSigninOperationResult Integer> onComplete) {
        ThreadUtils.assertOnUiThread();
        // Destroy the previous WebSigninTrackerBridge, if any, to avoid interference with the new
        // sign-in. A destroyed bridge doesn't report its result.
        cancel();
        WebSigninTrackerBridge webSigninTrackerBridge =
                mWebSigninTrackerBridgeFactory.createWithCoreAccountId(
                        mProfile,
                        accountId,
                        result -> {
                            // The bridge destroys itself after reporting its result.
                            mWebSigninTrackerBridge = null;
                            onWebSigninTrackerResult(result, onComplete);
                        });
        mWebSigninTrackerBridge = webSigninTrackerBridge;
        webSigninTrackerBridge.start();
    }

    /** Cancels the pending wait, if any. Its callback will not be invoked. */
    public void cancel() {
        if (mWebSigninTrackerBridge != null) {
            mWebSigninTrackerBridge.destroy();
            mWebSigninTrackerBridge = null;
        }
    }

    private void onWebSigninTrackerResult(
            @WebSigninTrackerResult int result,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        ThreadUtils.assertOnUiThread();
        switch (result) {
            case WebSigninTrackerResult.SUCCESS:
                onComplete.onResult(PostSigninOperationResult.SUCCESS);
                break;
            case WebSigninTrackerResult.AUTH_ERROR:
                SigninMetricsUtils.logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN, mAccessPoint);
                onComplete.onResult(PostSigninOperationResult.AUTH_ERROR);
                break;
            case WebSigninTrackerResult.OTHER_ERROR:
            case WebSigninTrackerResult.TIMEOUT:
                SigninMetricsUtils.logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN, mAccessPoint);
                onComplete.onResult(PostSigninOperationResult.OTHER_ERROR);
                break;
            default:
                throw new IllegalStateException("Unexpected result: " + result);
        }
    }
}
