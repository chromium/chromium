// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SigninAndHistoryOptInActivity} in modularized code. */
public interface SigninAndHistoryOptInActivityLauncher {
    /** Sign-in access points that are eligible to the sign-in and history opt-in flow. */
    @IntDef({
        SigninAccessPoint.RECENT_TABS,
        SigninAccessPoint.BOOKMARK_MANAGER,
        SigninAccessPoint.NTP_FEED_TOP_PROMO,
        SigninAccessPoint.NTP_FEED_BOTTOM_PROMO,
        SigninAccessPoint.SAFETY_CHECK,
        SigninAccessPoint.SETTINGS,
        SigninAccessPoint.WEB_SIGNIN,
        SigninAccessPoint.NTP_SIGNED_OUT_ICON,
        SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO,
        SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Launches the {@link SigninAndHistoryOptInActivity} from an eligible access point, shows error
     * UI if sign-in is not allowed.
     *
     * @param profile the current profile.
     * @param noAccountSigninMode The type of UI that should be shown for the sign-in step if
     *     there's no account on the device.
     * @param withAccountSigninMode The type of UI that should be shown for the sign-in step if
     *     there are 1+ accounts on the device.
     * @param historyOptInMode Whether the history opt-in should be always, optionally or never
     *     shown.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    public void launchActivityIfAllowed(
            Context context,
            Profile profile,
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.WithAccountSigninMode int withAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode,
            @AccessPoint int accessPoint);
}
