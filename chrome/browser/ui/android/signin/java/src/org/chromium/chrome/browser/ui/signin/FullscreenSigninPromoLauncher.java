// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Helper class responsible of launching the re-FRE with {@link SigninAndHistorySyncActivity}. */
public final class FullscreenSigninPromoLauncher {
    /**
     * Launches the {@link SigninAndHistoryOptInActivity} if it needs to be displayed.
     *
     * @param context The {@link Context} to launch the {@link SyncConsentActivity}.
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
        if (BuildInfo.getInstance().isAutomotive) {
            return false;
        }

        final SigninPreferencesManager prefManager = SigninPreferencesManager.getInstance();
        if (shouldLaunchPromo(profile, prefManager, currentMajorVersion)) {
            FullscreenSigninAndHistorySyncConfig config =
                    new FullscreenSigninAndHistorySyncConfig.Builder().build();
            @Nullable
            Intent intent =
                    signinAndHistorySyncActivityLauncher.createFullscreenSigninIntent(
                            context, profile, config, SigninAccessPoint.SIGNIN_PROMO);
            if (intent != null) {
                context.startActivity(intent);
            }
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            final List<CoreAccountInfo> coreAccountInfos =
                    AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                            AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
            prefManager.setSigninPromoLastAccountEmails(
                    new HashSet<>(AccountUtils.toAccountEmails(coreAccountInfos)));
            return true;
        }
        return false;
    }

    private static boolean shouldLaunchPromo(
            Profile profile, SigninPreferencesManager prefManager, final int currentMajorVersion) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO)) {
            return true;
        }
        final int lastPromoMajorVersion = prefManager.getSigninPromoLastShownVersion();
        if (lastPromoMajorVersion == 0) {
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            return false;
        }

        if (currentMajorVersion < lastPromoMajorVersion + 2) {
            // Promo can be shown at most once every 2 Chrome major versions.
            return false;
        }

        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) != null) {
            // Don't show if user is signed in and syncing.
            return false;
        }

        if (!TextUtils.isEmpty(
                UserPrefs.get(profile).getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))) {
            // Don't show if user has manually signed out.
            return false;
        }

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        final List<CoreAccountInfo> coreAccountInfos =
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        accountManagerFacade.getCoreAccountInfos());
        if (coreAccountInfos.isEmpty()) {
            // Don't show if the account list isn't available yet or there are no accounts in it.
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS)) {
            return false;
        }

        final List<String> currentAccountEmails = AccountUtils.toAccountEmails(coreAccountInfos);
        final Set<String> previousAccountEmails = prefManager.getSigninPromoLastAccountEmails();
        // Don't show if no new accounts have been added after the last time promo was shown.
        return previousAccountEmails == null
                || !previousAccountEmails.containsAll(currentAccountEmails);
    }

    private FullscreenSigninPromoLauncher() {}
}
