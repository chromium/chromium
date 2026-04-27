// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

/** A controller that displays a blocking UI when the user signs out. */
@NullMarked
public class ForcedSigninController implements IdentityManager.Observer {
    private final Context mContext;
    private final Profile mProfile;
    private final SigninAndHistorySyncActivityLauncher mLauncher;
    private final IdentityManager mIdentityManager;

    public ForcedSigninController(
            Context context, Profile profile, SigninAndHistorySyncActivityLauncher launcher) {
        mContext = context;
        mProfile = profile;
        mLauncher = launcher;
        mIdentityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
        mIdentityManager.addObserver(this);
    }

    public void destroy() {
        mIdentityManager.removeObserver(this);
    }

    /** Whether the forced sign-in policy is enabled. */
    public static boolean isForcedSigninPolicyEnabled() {
        boolean isPolicyEnabled =
                assumeNonNull(LocalStatePrefs.get()).getBoolean(Pref.FORCE_BROWSER_SIGNIN);
        return SigninFeatureMap.isEnabled(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
                && isPolicyEnabled;
    }

    /** Whether the forced sign-in screen should be displayed for the given profile. */
    public static boolean shouldDisplayForcedSignin(Profile profile) {
        boolean isSignedIn =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile))
                        .hasPrimaryAccount();
        return isForcedSigninPolicyEnabled() && !isSignedIn;
    }

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        if (eventDetails.getEventTypeFor() == PrimaryAccountChangeEvent.Type.CLEARED) {
            FullscreenSigninPromoLauncher.launchPromoIfForced(mContext, mProfile, mLauncher);
        }
    }
}
