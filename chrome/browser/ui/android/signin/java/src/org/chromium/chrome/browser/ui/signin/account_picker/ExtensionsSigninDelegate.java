// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.WebSigninTrackerBridge;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Implementation of {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate} for the sign-in
 * flow triggered by the {@code chrome.identity} extensions API.
 *
 * <p>After the sign-in, the extensions API may need to load a Gaia page (e.g. the remote consent
 * page) which requires the signed-in account to be available in the cookie jar. Like for web
 * sign-in, the sign-in bottom sheet is kept open with a spinner until the cookies are minted, and
 * shows an error screen if this fails.
 *
 * <p>TODO(crbug.com/539487000): Report the result of the sign-in flow to the extensions API.
 */
@NullMarked
public class ExtensionsSigninDelegate
        implements BottomSheetSigninAndHistorySyncCoordinator.Delegate {
    private final SigninCookieWaiter mCookieWaiter;

    /**
     * @param profile The profile in which the sign-in happens.
     * @param webSigninTrackerBridgeFactory A {@link WebSigninTrackerBridge.Factory} to create
     *     {@link WebSigninTrackerBridge} instances.
     */
    public ExtensionsSigninDelegate(
            Profile profile, WebSigninTrackerBridge.Factory webSigninTrackerBridgeFactory) {
        mCookieWaiter =
                new SigninCookieWaiter(
                        profile, webSigninTrackerBridgeFactory, SigninAccessPoint.EXTENSIONS);
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void runPostSigninAction(
            CoreAccountInfo signedInAccount,
            @Nullable DelegateContext delegateContext,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        mCookieWaiter.waitForCookies(signedInAccount.getId(), onComplete);
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void onFlowComplete(SigninAndHistorySyncCoordinator.Result result) {
        // The flow may complete while the cookie wait is still pending, e.g. if the user dismisses
        // the bottom sheet during the wait. Its result must then be ignored.
        mCookieWaiter.cancel();
    }
}
