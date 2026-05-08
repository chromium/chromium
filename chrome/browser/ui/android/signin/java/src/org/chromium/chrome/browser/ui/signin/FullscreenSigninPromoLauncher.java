// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.DeviceInfo;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashSet;
import java.util.List;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Helper class responsible of launching the re-FRE with {@link SigninAndHistorySyncActivity}. */
@NullMarked
public final class FullscreenSigninPromoLauncher {
    /**
     * Launches the {@link SigninAndHistorySyncActivity} if it needs to be displayed.
     *
     * @param context The {@link Context} to launch the {@link SigninAndHistorySyncActivity}.
     * @param profile The active user profile.
     * @param signinAndHistorySyncActivityLauncher launcher used to launch the {@link
     *     SigninAndHistorySyncActivity}.
     * @param currentMajorVersion The current major version of Chrome.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchPromoIfNeeded(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            final int currentMajorVersion) {
        final SigninPreferencesManager prefManager = SigninPreferencesManager.getInstance();
        if (!shouldLaunchPromo(profile, prefManager, currentMajorVersion)) {
            return false;
        }

        if (!createAndLaunchActivity(
                context,
                profile,
                signinAndHistorySyncActivityLauncher,
                SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO)) {
            return false;
        }

        prefManager.setSigninPromoLastShownTimeWithRandomOffset(getCurrentTimeWithRandomOffset());
        prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
        var accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(
                        AccountManagerFacadeProvider.getInstance().getAccounts());
        prefManager.setSigninPromoLastAccountEmails(
                new HashSet<>(AccountUtils.toAccountEmails(accounts)));
        return true;
    }

    /**
     * Launches the {@link SigninAndHistorySyncActivity} when forcing the display.
     *
     * @param context The {@link Context} to launch the {@link SigninAndHistorySyncActivity}.
     * @param profile The active user profile.
     * @param signinAndHistorySyncActivityLauncher launcher used to launch the {@link
     *     SigninAndHistorySyncActivity}.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchPromoIfForced(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher) {
        boolean shouldDisplayForForcedSigninPolicy =
                ForcedSigninController.shouldDisplayForcedSignin(profile);
        if (!SigninFeatureMap.isEnabled(SigninFeatures.FORCE_STARTUP_SIGNIN_PROMO)
                && !shouldDisplayForForcedSigninPolicy) {
            return false;
        }
        return createAndLaunchActivity(
                context,
                profile,
                signinAndHistorySyncActivityLauncher,
                shouldDisplayForForcedSigninPolicy
                        ? SigninAccessPoint.FORCED_SIGNIN
                        : SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO);
    }

    private static boolean createAndLaunchActivity(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            @SigninAccessPoint int accessPoint) {
        FullscreenSigninAndHistorySyncConfig config =
                new FullscreenSigninAndHistorySyncConfig.Builder(
                                context.getString(R.string.signin_fre_title),
                                context.getString(R.string.signin_fre_subtitle),
                                context.getString(R.string.signin_fre_stay_signed_out_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .build();
        @Nullable Intent intent =
                signinAndHistorySyncActivityLauncher.createFullscreenSigninIntent(
                        context, profile, config, accessPoint);
        if (intent == null) {
            return false;
        }
        context.startActivity(intent);
        return true;
    }

    private static boolean shouldLaunchPromo(
            Profile profile, SigninPreferencesManager prefManager, final int currentMajorVersion) {
        if (DeviceInfo.isAutomotive()) {
            return false;
        }

        final long lastShowTimeWithRandomOffset =
                prefManager.getSigninPromoLastShownTimeWithRandomOffset();
        boolean useDate =
                SigninFeatureMap.isEnabled(SigninFeatures.FULLSCREEN_SIGN_IN_PROMO_USE_DATE);

        // Set the last shown time if it was never set before or if it's in the future (in the case
        // of device time change).
        if (lastShowTimeWithRandomOffset < 0
                || lastShowTimeWithRandomOffset > TimeUtils.currentTimeMillis()) {
            prefManager.setSigninPromoLastShownTimeWithRandomOffset(
                    getCurrentTimeWithRandomOffset());
            // Don't show if next show time was never recorded in the past.
            if (useDate) {
                return false;
            }
        }
        if (useDate) {
            int interval =
                    SigninFeatureMap.getInstance()
                            .getFieldTrialParamByFeatureAsInt(
                                    SigninFeatures.FULLSCREEN_SIGN_IN_PROMO_USE_DATE,
                                    "interval",
                                    -1);
            if (interval == -1) {
                throw new IllegalStateException(
                        "interval param should be available with feature enabled");
            }
            final long nextShowTime =
                    TimeUnit.DAYS.toMillis(interval) + lastShowTimeWithRandomOffset;
            if (nextShowTime > TimeUtils.currentTimeMillis()) {
                return false;
            }
        }

        final int lastPromoMajorVersion = prefManager.getSigninPromoLastShownVersion();
        if (lastPromoMajorVersion == 0) {
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            if (!useDate) {
                return false;
            }
        }
        if (!useDate && currentMajorVersion < lastPromoMajorVersion + 2) {
            // Promo can be shown at most once every 2 Chrome major versions.
            return false;
        }

        if (!TextUtils.isEmpty(
                UserPrefs.get(profile).getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))) {
            // Don't show if user has manually signed out.
            return false;
        }

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        var accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(accountManagerFacade.getAccounts());
        if (accounts.isEmpty()) {
            // Don't show if the account list isn't available yet or there are no accounts in it.
            return false;
        }

        final List<String> currentAccountEmails = AccountUtils.toAccountEmails(accounts);
        final Set<String> previousAccountEmails = prefManager.getSigninPromoLastAccountEmails();
        // Don't show if no new accounts have been added after the last time promo was shown.
        return previousAccountEmails == null
                || !previousAccountEmails.containsAll(currentAccountEmails);
    }

    // Returns a random time offset in the past between [0..14) days in milliseconds.
    private static long getCurrentTimeWithRandomOffset() {
        int days = new Random().nextInt(14);
        return TimeUtils.currentTimeMillis() - TimeUnit.DAYS.toMillis(days);
    }

    private FullscreenSigninPromoLauncher() {}
}
