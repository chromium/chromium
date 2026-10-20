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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.FeatureProperties;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.UiFormat;
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

        // Pass `shouldCallTrackerDismissed` based on whether the interruptive cooldown group is
        // enabled, because this startup promo flow gates display on
        // `shouldTriggerFullscreenPromo()`
        // (which calls `Tracker#shouldTriggerHelpUi` when the flag is enabled), so
        // `FullscreenSigninAndHistorySyncCoordinator#destroy()` must pair it with a call to
        // `Tracker#dismissed()`. Other fullscreen flows (e.g. `launchPromoIfForced` and
        // `launchForcedSigninFlow`) bypass the tracker check and pass `false`.
        @Nullable Intent intent =
                createActivityIntent(
                        context,
                        profile,
                        signinAndHistorySyncActivityLauncher,
                        SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO,
                        /* shouldCallTrackerDismissed= */ ChromeFeatureList
                                .sInterruptiveIphCooldownGroup
                                .isEnabled());
        if (intent == null) {
            return false;
        }

        // Check `shouldTriggerFullscreenPromo` only after `createActivityIntent` returns a
        // non-null Intent so we don't trigger the FET feature (and start the cooldown) when the
        // sign-in/history-sync UI cannot be shown.
        if (!shouldTriggerFullscreenPromo(profile)) {
            return false;
        }

        context.startActivity(intent);

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
     * Launches the {@link SigninAndHistorySyncActivity} when forcing the display with a flag.
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
        if (!SigninFeatureMap.isEnabled(SigninFeatures.FORCE_STARTUP_SIGNIN_PROMO)) {
            return false;
        }
        return createAndLaunchActivity(
                context,
                profile,
                signinAndHistorySyncActivityLauncher,
                SigninAccessPoint.FULLSCREEN_SIGNIN_PROMO);
    }

    /**
     * Launches the {@link SigninAndHistorySyncActivity} for the forced sign-in flow. Callers are
     * responsible for checking whether the forced sign-in flow should be displayed, see {@link
     * ForcedSigninController#showFullscreenSigninPromptIfRequired()}.
     *
     * @param context The {@link Context} to launch the {@link SigninAndHistorySyncActivity}.
     * @param profile The active user profile.
     * @param signinAndHistorySyncActivityLauncher launcher used to launch the {@link
     *     SigninAndHistorySyncActivity}.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchForcedSigninFlow(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher) {
        return createAndLaunchActivity(
                context,
                profile,
                signinAndHistorySyncActivityLauncher,
                SigninAccessPoint.FORCED_SIGNIN);
    }

    /**
     * Creates an {@link Intent} for launching {@link SigninAndHistorySyncActivity}.
     *
     * @param shouldCallTrackerDismissed Whether this flow is gated by the Feature Engagement {@link
     *     Tracker} and must call {@link Tracker#dismissed} when the fullscreen coordinator is
     *     destroyed.
     */
    private static @Nullable Intent createActivityIntent(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            @SigninAccessPoint int accessPoint,
            boolean shouldCallTrackerDismissed) {
        FullscreenSigninAndHistorySyncConfig config =
                FullscreenSigninAndHistorySyncConfig.builder(
                                context.getString(R.string.signin_fre_title),
                                context.getString(R.string.signin_fre_subtitle),
                                context.getString(R.string.signin_fre_stay_signed_out_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .shouldCallTrackerDismissed(shouldCallTrackerDismissed)
                        .build();
        return signinAndHistorySyncActivityLauncher.createFullscreenSigninIntent(
                context, profile, config, accessPoint);
    }

    private static boolean createAndLaunchActivity(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            @SigninAccessPoint int accessPoint) {
        @Nullable Intent intent =
                createActivityIntent(
                        context,
                        profile,
                        signinAndHistorySyncActivityLauncher,
                        accessPoint,
                        /* shouldCallTrackerDismissed= */ false);
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

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        if (signinManager == null
                || !signinManager.isSigninSupported(/* requireUpdatedPlayServices= */ true)) {
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

    /**
     * Queries the Feature Engagement {@link Tracker} to check whether the fullscreen sign-in promo
     * is allowed to be shown under the enterprise promotion policy and the cross-promo interruptive
     * IPH cooldown group rules (which rate-limit how frequently interruptive promos can be shown
     * across Chrome). If this returns {@code true}, {@link Tracker#dismissed} must be called when
     * the promo UI is dismissed.
     */
    private static boolean shouldTriggerFullscreenPromo(Profile profile) {
        if (!ChromeFeatureList.sInterruptiveIphCooldownGroup.isEnabled()) {
            return true;
        }
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        FeatureProperties featureProperties =
                new FeatureProperties(
                        UiFormat.FRE,
                        /* isBlockedByEnterprisePolicy= */ true,
                        /* isBlockedByCooldownGroup= */ true,
                        /* blocksCooldownGroup= */ true);
        return tracker.shouldTriggerHelpUi(
                FeatureConstants.FULLSCREEN_SIGNIN_PROMO_FEATURE, featureProperties);
    }

    // Returns a random time offset in the past between [0..14) days in milliseconds.
    private static long getCurrentTimeWithRandomOffset() {
        int days = new Random().nextInt(14);
        return TimeUtils.currentTimeMillis() - TimeUnit.DAYS.toMillis(days);
    }

    private FullscreenSigninPromoLauncher() {}
}
