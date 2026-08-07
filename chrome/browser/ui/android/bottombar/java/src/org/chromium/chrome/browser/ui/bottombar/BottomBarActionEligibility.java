// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.base.LocaleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.actions.ActionId;

import java.util.Locale;
import java.util.function.Supplier;

/** Helper class to resolve the eligibility of bottom bar actions based on profile and country. */
@NullMarked
public class BottomBarActionEligibility {

    /** Represents a sentinel value indicating that no action is eligible. */
    static final int ACTION_NONE = -1;

    private static @Nullable Supplier<String> sCountrySupplier;

    /** Sets the supplier for obtaining the latest variations country code. */
    public static void setCountrySupplier(@Nullable Supplier<String> supplier) {
        sCountrySupplier = supplier;
    }

    /**
     * Returns whether the GLIC bottom bar setting toggle should be shown for the given profile.
     */
    public static boolean shouldShowBottomBarGlicSetting(@Nullable Profile profile) {
        if (profile == null) {
            return false;
        }
        return GlicEnabling.shouldShowSettingsPage(profile)
                && isGlicAllowedInCountry()
                && BottomBarConfigUtils.isGlicSettingToggleParamEnabled();
    }

    /**
     * Resolves which action (if any) should be displayed in the bottom bar's shared extra
     * container.
     *
     * @param profile The current user profile.
     * @return The eligible {@link ActionId} (either {@link ActionId#GLIC} or {@link
     *     ActionId#AI_MODE}), or {@link #ACTION_NONE} if no action is eligible.
     */
    /* package-private */ static int getEligibleExtraAction(@Nullable Profile profile) {
        if (profile == null) {
            return ACTION_NONE;
        }

        String country = getCountryCode();
        boolean bypassGlic = BottomBarConfigUtils.bypassGlicGeofencing();

        // 1. GLIC (Gemini): Check if GLIC is enabled for this profile and allowed in country.
        if (GlicEnabling.isEnabledForProfile(profile) && isGlicAllowedInCountry()) {
            // If enterprise policy forcibly enables GLIC, always show GLIC button.
            // (Note: If policy forcibly disables GLIC, GlicEnabling.isEnabledForProfile is false
            // above).
            if (GlicEnabling.isPolicyEnforced(profile)) {
                return ActionId.GLIC;
            }
            // Respect user setting: show GLIC if toggle is ON, or show nothing if OFF.
            return BottomBarConfigUtils.isGlicButtonEnabled() ? ActionId.GLIC : ACTION_NONE;
        }

        // 2. Soon to be Launched: If country is GLIC Soon to be Launched (and not bypassed) -> Show
        // nothing.
        if (!bypassGlic && BottomBarGeofencingConfig.GLIC_SOON_COUNTRIES.contains(country)) {
            return ACTION_NONE;
        }

        // 3. AI Mode Fallback: If DSE is Google AND (country is AIM Allowed OR bypass is true) ->
        // Show AI Mode.
        boolean bypassAim = BottomBarConfigUtils.bypassAimGeofencing();
        boolean isDseGoogle = isDefaultSearchEngineGoogle(profile);
        if (isDseGoogle
                && (bypassAim
                        || BottomBarGeofencingConfig.AIM_ALLOWED_COUNTRIES.contains(country))) {
            return ActionId.AI_MODE;
        }

        // 4. Otherwise: Show nothing.
        return ACTION_NONE;
    }

    private static String getCountryCode() {
        String country = null;
        if (sCountrySupplier != null) {
            country = sCountrySupplier.get();
        }
        if (country == null || country.isEmpty()) {
            country = LocaleUtils.getDefaultCountryCode();
        }
        return country != null ? country.toLowerCase(Locale.US) : "";
    }

    /** Returns whether GLIC is allowed in the user's country based on geofencing. */
    private static boolean isGlicAllowedInCountry() {
        String country = getCountryCode();
        boolean bypassGlic = BottomBarConfigUtils.bypassGlicGeofencing();
        return bypassGlic || BottomBarGeofencingConfig.GLIC_ALLOWED_COUNTRIES.contains(country);
    }

    private static boolean isDefaultSearchEngineGoogle(Profile profile) {
        return TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle();
    }
}
