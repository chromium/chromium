// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.GlicIneligibilityReason;

import java.util.Locale;

/** Helper class to resolve the eligibility of bottom bar actions based on profile and country. */
@NullMarked
public class BottomBarActionEligibility {

    /** Represents a sentinel value indicating that no action is eligible. */
    public static final @ActionId int ACTION_NONE = ActionId.NONE;

    private static @Nullable @ActionId Integer sCachedCandidateExtraAction;

    /** Returns the currently cached candidate extra action, or null if uninitialized. */
    public static @Nullable @ActionId Integer getCachedCandidateExtraAction() {
        return sCachedCandidateExtraAction;
    }

    /** Sets the cached candidate extra action for testing. */
    public static void setCachedCandidateExtraActionForTesting(
            @Nullable @ActionId Integer candidate) {
        sCachedCandidateExtraAction = candidate;
        ResettersForTesting.register(() -> sCachedCandidateExtraAction = null);
    }

    /**
     * Resolves the variations country code, falling back to the default locale country code on
     * local development builds if the variations country is not yet populated or empty. In
     * production builds, returns the raw variations country (or null/empty if unpopulated).
     *
     * @param variationsCountry The raw country code from variations service (or null).
     * @return The resolved country code, or null/empty if unpopulated.
     */
    public static @Nullable String resolveCountryCodeWithLocalDevFallback(
            @Nullable String variationsCountry) {
        if ((variationsCountry == null || variationsCountry.isEmpty())
                && VersionInfo.isLocalBuild()) {
            return LocaleUtils.getDefaultCountryCode();
        }
        return variationsCountry;
    }

    /**
     * Returns whether candidate extra action resolution can proceed with the given inputs.
     *
     * <p>Resolution is ready if:
     *
     * <ol>
     *   <li>Profile is non-null AND GLIC is disabled for profile (always {@link #ACTION_NONE}).
     *   <li>Profile is non-null AND GLIC is enabled for profile and geofencing is bypassed (always
     *       {@link ActionId#GLIC}).
     *   <li>Profile is non-null AND a non-empty country code is provided.
     * </ol>
     *
     * @param profile The current user profile.
     * @param country The variations country code, or null if pending.
     * @return True if candidate resolution can proceed deterministically.
     */
    public static boolean isCandidateResolutionReady(
            @Nullable Profile profile, @Nullable String country) {
        if (profile == null) {
            return false;
        }

        Profile originalProfile = profile.getOriginalProfile();
        boolean bypassGlic = BottomBarConfigUtils.bypassGlicGeofencing();
        boolean isGlicProfileEnabled = GlicEnabling.isEnabledForProfile(originalProfile);

        if (!isGlicProfileEnabled || bypassGlic) {
            return true;
        }

        return !normalizeCountry(country).isEmpty();
    }

    /**
     * Returns whether the GLIC bottom bar setting toggle should be shown for the given profile.
     *
     * <p>The toggle is only shown if candidate resolution has already resolved GLIC as the
     * candidate extra action for the bottom bar. If candidate resolution has not occurred yet or
     * resolved to another action / none, returns false.
     *
     * @param profile The current user profile.
     * @return True if the setting toggle should be shown.
     */
    public static boolean shouldShowBottomBarGlicSetting(@Nullable Profile profile) {
        if (profile == null) {
            return false;
        }
        if (sCachedCandidateExtraAction == null || sCachedCandidateExtraAction != ActionId.GLIC) {
            return false;
        }
        Profile originalProfile = profile.getOriginalProfile();
        return BottomBarConfigUtils.isGlicSettingToggleParamEnabled()
                && GlicEnabling.shouldShowSettingsPage(originalProfile);
    }

    /**
     * Resolves the static candidate action (if any) that can be displayed in the bottom bar's
     * shared extra container for the given profile and country.
     *
     * @param profile The current user profile.
     * @param country The variations country code.
     * @return The candidate {@link ActionId} ({@link ActionId#GLIC}), or {@link #ACTION_NONE} if no
     *     action is eligible.
     */
    @ActionId
    public static int getCandidateExtraAction(@Nullable Profile profile, @Nullable String country) {
        if (profile == null) {
            return ACTION_NONE;
        }

        Profile originalProfile = profile.getOriginalProfile();
        String normalizedCountry = normalizeCountry(country);
        boolean isGlicAllowed = isGlicAllowedInCountry(normalizedCountry);
        boolean isGlicProfileEnabled = GlicEnabling.isEnabledForProfile(originalProfile);

        // Check if GLIC is enabled for this profile and allowed in country.
        if (isGlicProfileEnabled && isGlicAllowed) {
            sCachedCandidateExtraAction = ActionId.GLIC;
            if (GlicEnabling.isPolicyEnforced(originalProfile)) {
                return ActionId.GLIC;
            }
            if (BottomBarConfigUtils.isGlicButtonEnabled()) {
                return ActionId.GLIC;
            }
            BottomBarMetrics.recordGlicIneligibilityReason(
                    GlicIneligibilityReason.USER_DISABLED_IN_SETTINGS);
            return ACTION_NONE;
        }

        if (!isGlicProfileEnabled) {
            BottomBarMetrics.recordGlicIneligibilityReason(
                    GlicIneligibilityReason.PROFILE_INELIGIBLE);
        } else if (!isGlicAllowed) {
            BottomBarMetrics.recordGlicIneligibilityReason(
                    GlicIneligibilityReason.COUNTRY_GEOFENCED);
        }

        sCachedCandidateExtraAction = ACTION_NONE;
        return ACTION_NONE;
    }

    /**
     * Returns whether GLIC is allowed in the user's country based on geofencing.
     *
     * @param country The variations country code.
     * @return True if GLIC is allowed or geofencing is bypassed.
     */
    public static boolean isGlicAllowedInCountry(@Nullable String country) {
        if (BottomBarConfigUtils.bypassGlicGeofencing()) {
            return true;
        }
        String normalizedCountry = normalizeCountry(country);
        if (normalizedCountry.isEmpty()) {
            return false;
        }
        return BottomBarGeofencingConfig.GLIC_ALLOWED_COUNTRIES.contains(normalizedCountry);
    }

    private static String normalizeCountry(@Nullable String country) {
        return country != null ? country.trim().toLowerCase(Locale.US) : "";
    }
}
