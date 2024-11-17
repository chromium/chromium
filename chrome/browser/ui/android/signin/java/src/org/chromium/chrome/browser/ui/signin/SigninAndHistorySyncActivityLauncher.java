// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SigninAndHistorySyncActivity} in modularized code. */
public interface SigninAndHistorySyncActivityLauncher {
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
        SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Create {@Intent} for the {@link SigninAndHistorySyncActivity} from an eligible access point,
     * Show an error if the intent can't be created.
     *
     * @param profile the current profile.
     * @param bottomSheetStrings the strings shown in the sign-in bottom sheet.
     * @param noAccountSigninMode The type of UI that should be shown for the sign-in step if
     *     there's no account on the device.
     * @param withAccountSigninMode The type of UI that should be shown for the sign-in step if
     *     there are 1+ accounts on the device.
     * @param historyOptInMode Whether the history opt-in should be always, optionally or never
     *     shown.
     * @param accessPoint The access point from which the sign-in was triggered.
     * @param selectedCoreAccountId The account that should be displayed in the sign-in bottom
     *     sheet. If null, the default account will be displayed.
     */
    @MainThread
    @Nullable
    Intent createBottomSheetSigninIntentOrShowError(
            Context context,
            Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                    int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @AccessPoint int accessPoint,
            @Nullable CoreAccountId selectedCoreAccountId);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Does not show any error if the intent can't be
     * created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable
    Intent createFullscreenSigninIntent(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Show an error if the intent can't be created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable
    Intent createFullscreenSigninIntentOrShowError(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);
}
