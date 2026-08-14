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
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.AimIneligibilityReason;
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
     *   <li>Profile is non-null AND a non-empty country code is provided.
     *   <li>Profile is non-null AND the outcome is unconditionally deterministic without country:
     *       <ul>
     *         <li>GLIC is enabled for profile and bypassGlic is true.
     *         <li>GLIC is disabled for profile and bypassAim is true.
     *       </ul>
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
        String normalizedCountry = normalizeCountry(country);
        boolean bypassGlic = BottomBarConfigUtils.bypassGlicGeofencing();
        boolean bypassAim =
                BottomBarConfigUtils.isAimEnabled()
                        && BottomBarConfigUtils.bypassAimGeofencing();
        boolean isGlicProfileEnabled = GlicEnabling.isEnabledForProfile(originalProfile);

        // Case 1: GLIC is enabled for profile and GLIC geofencing is bypassed -> Always GLIC.
        if (isGlicProfileEnabled && bypassGlic) {
            return true;
        }

        // Case 2: GLIC is disabled for profile and AIM geofencing is bypassed -> Always AIM.
        if (!isGlicProfileEnabled && bypassAim) {
            return true;
        }

        // Case 3: Country code is required to resolve geofenced allowlists/soonlists.
        return !normalizedCountry.isEmpty();
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
     * @return The candidate {@link ActionId} (either {@link ActionId#GLIC} or {@link
     *     ActionId#AI_MODE}), or {@link #ACTION_NONE} if no action is eligible.
     */
    @ActionId
    public static int getCandidateExtraAction(@Nullable Profile profile, @Nullable String country) {
        if (profile == null) {
            return ACTION_NONE;
        }

        Profile originalProfile = profile.getOriginalProfile();
        String normalizedCountry = normalizeCountry(country);
        boolean bypassGlic = BottomBarConfigUtils.bypassGlicGeofencing();
        boolean isGlicAllowed = isGlicAllowedInCountry(normalizedCountry);
        boolean isGlicProfileEnabled = GlicEnabling.isEnabledForProfile(originalProfile);

        // 1. GLIC (Gemini): Check if GLIC is enabled for this profile and allowed in country.
        if (isGlicProfileEnabled && isGlicAllowed) {
            sCachedCandidateExtraAction = ActionId.GLIC;
            BottomBarMetrics.recordAimIneligibilityReason(AimIneligibilityReason.PREEMPTED_BY_GLIC);
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

        // 2. Soon to be Launched: If country is GLIC Soon to be Launched (and not bypassed) -> Show
        // nothing.
        if (!bypassGlic
                && BottomBarGeofencingConfig.GLIC_SOON_COUNTRIES.contains(normalizedCountry)) {
            sCachedCandidateExtraAction = ACTION_NONE;
            BottomBarMetrics.recordAimIneligibilityReason(
                    AimIneligibilityReason.COUNTRY_IN_GLIC_SOON_LIST);
            return ACTION_NONE;
        }

        // 3. AI Mode: Check if AIM feature flag is enabled AND (country is AIM Allowed OR bypass is
        // true).
        if (!BottomBarConfigUtils.isAimEnabled()) {
            sCachedCandidateExtraAction = ACTION_NONE;
            BottomBarMetrics.recordAimIneligibilityReason(
                    AimIneligibilityReason.FEATURE_FLAG_DISABLED);
            return ACTION_NONE;
        }

        boolean bypassAim = BottomBarConfigUtils.bypassAimGeofencing();
        if (bypassAim || isAimAllowedInCountry(normalizedCountry)) {
            sCachedCandidateExtraAction = ActionId.AI_MODE;
            return ActionId.AI_MODE;
        }

        sCachedCandidateExtraAction = ACTION_NONE;
        BottomBarMetrics.recordAimIneligibilityReason(AimIneligibilityReason.COUNTRY_GEOFENCED);
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

    /**
     * Returns whether AI Mode is allowed in the user's country based on geofencing.
     *
     * @param country The variations country code.
     * @return True if AI Mode is allowed or geofencing is bypassed.
     */
    public static boolean isAimAllowedInCountry(@Nullable String country) {
        if (BottomBarConfigUtils.bypassAimGeofencing()) {
            return true;
        }
        String normalizedCountry = normalizeCountry(country);
        if (normalizedCountry.isEmpty()) {
            return false;
        }
        return BottomBarGeofencingConfig.AIM_ALLOWED_COUNTRIES.contains(normalizedCountry);
    }

    private static String normalizeCountry(@Nullable String country) {
        return country != null ? country.trim().toLowerCase(Locale.US) : "";
    }
}
