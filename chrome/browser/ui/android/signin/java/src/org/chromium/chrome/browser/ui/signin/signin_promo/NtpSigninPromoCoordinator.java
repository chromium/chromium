// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.view.View;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;

/** Coordinator for the seamless sign-in promo card in NTP. */
@NullMarked
public class NtpSigninPromoCoordinator {
    private final SigninPromoCoordinator mSigninPromoCoordinator;
    private final PersonalizedSigninPromoView mSigninPromoView;

    /**
     * Creates an instance of the {@link NtpSigninPromoCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param profile A {@link Profile} object to access identity services. This must be the
     *     original profile, not the incognito one.
     * @param launcher A {@SigninAndHistorySyncActivityLauncher} for the initialization of {@link
     *     SigninPromoDelegate}.
     * @param signinPromoViewContainerStub The ViewStub that contains the layout element in which
     *     the sign-in promo will be inflated.
     */
    public NtpSigninPromoCoordinator(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            ViewStub signinPromoViewContainerStub) {
        mSigninPromoCoordinator =
                new SigninPromoCoordinator(
                        context,
                        profile,
                        new NtpSigninPromoDelegate(
                                context, profile, launcher, this::onPromoStateChange));
        mSigninPromoView = (PersonalizedSigninPromoView) signinPromoViewContainerStub.inflate();
        mSigninPromoView.setCardBackgroundResource(R.drawable.home_surface_ui_background);
        mSigninPromoCoordinator.setView(mSigninPromoView);
        onPromoStateChange();
    }

    public void destroy() {
        mSigninPromoCoordinator.destroy();
    }

    private void onPromoStateChange() {
        mSigninPromoView.setVisibility(
                mSigninPromoCoordinator.canShowPromo() ? View.VISIBLE : View.GONE);
    }
}
