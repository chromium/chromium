// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility class for determining the configuration of the bottom bar. */
@NullMarked
public class BottomBarConfigUtils {
    public static final int DEFAULT_BOTTOM_BAR_HEIGHT_DP = 56;
    public static final int MIN_BOTTOM_BAR_HEIGHT_DP = 48;
    public static final int MAX_BOTTOM_BAR_HEIGHT_DP = 60;

    /**
     * In-memory override holding the bottom bar state that was already used at startup when Feature
     * Engagement releases a conflicting saved default mid-process. Keeps bottom bar visibility
     * stable and suppresses education until the next app start.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    private static @TriState int sReleasedPinForProcess = TriState.NOT_SET;

    /**
     * Saved bottom bar default and onboarding state for AndroidBottomBar experiment users.
     * Persisted in SharedPreferences, so do not renumber or reuse entries.
     */
    // TODO(crbug.com/582747594): Migrate KEPT_* entries to BOTTOM_BAR_ENABLED (or keep reading
    // them) before removing this state when AndroidBottomBar is enabled by default, otherwise
    // users with a kept default will have their bottom bar flipped.
    // LINT.IfChange(AndroidBottomBarDefaultPinState)
    @IntDef({
        DefaultPinState.NONE,
        DefaultPinState.PENDING_OFF,
        DefaultPinState.PENDING_ON,
        DefaultPinState.KEPT_OFF,
        DefaultPinState.KEPT_ON,
        DefaultPinState.RELEASED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultPinState {
        // No saved default. Follow default_user_enabled.
        int NONE = 0;
        // Default saved at startup before Feature Engagement initialized. Acts as a saved default
        // until backfill promotes it to KEPT_* (if education was already seen) or releases it to
        // RELEASED (once education can show).
        int PENDING_OFF = 1;
        int PENDING_ON = 2;
        // User already saw the onboarding flow. Keep this saved default across launches.
        int KEPT_OFF = 3;
        int KEPT_ON = 4;
        // User had not seen onboarding and education can show. Follow default_user_enabled.
        int RELEASED = 5;
        int COUNT = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarDefaultPinState)

    /**
     * Reason for a {@link DefaultPinState} write. Recorded in
     * Android.BottomBar.DefaultPinState.Transition.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    // LINT.IfChange(AndroidBottomBarDefaultPinTransition)
    @IntDef({
        DefaultPinTransition.PINNED_OFF,
        DefaultPinTransition.PINNED_ON,
        DefaultPinTransition.KEPT_OFF_ON_EDUCATION,
        DefaultPinTransition.KEPT_ON_ON_EDUCATION,
        DefaultPinTransition.KEPT_OFF_ON_BACKFILL,
        DefaultPinTransition.KEPT_ON_ON_BACKFILL,
        DefaultPinTransition.RELEASED,
        DefaultPinTransition.RELEASED_AND_HELD
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DefaultPinTransition {
        // default_user_enabled saved as a pending pin on the first start with pinning.
        int PINNED_OFF = 0;
        int PINNED_ON = 1;
        // Education showed, so the state it showed with is kept.
        int KEPT_OFF_ON_EDUCATION = 2;
        int KEPT_ON_ON_EDUCATION = 3;
        // Feature Engagement says education was seen, so the pending pin is kept.
        int KEPT_OFF_ON_BACKFILL = 4;
        int KEPT_ON_ON_BACKFILL = 5;
        // Never seen and education can show, so the pin is released.
        int RELEASED = 6;
        // Released, but the bar would flip, so it is held for this run.
        int RELEASED_AND_HELD = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidBottomBarDefaultPinTransition)

    private BottomBarConfigUtils() {}

    /**
     * Returns whether the bottom bar is enabled, i.e. the device and feature flag are eligible and
     * the user has not turned it off in Settings. Most callers checking whether the bottom bar is
     * active should use this method.
     */
    public static boolean isBottomBarEnabled(Context context) {
        return isBottomBarEligible(context) && isBottomBarUserEnabled();
    }

    /**
     * Returns whether the bottom bar is eligible on this device, i.e. the feature flag is enabled
     * and the form factor supports it. This does not take the user's Settings choice into account.
     */
    public static boolean isBottomBarEligible(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && !DeviceInfo.isAutomotive()
                && ChromeFeatureList.sAndroidBottomBar.isEnabled();
    }

    /**
     * Returns whether the "Toolbar" (bottom toolbar) switch should be offered in Settings: the
     * bottom bar must be eligible on this device and the show_settings_toggle param must be on.
     * Never true when the AndroidBottomBar flag is off.
     */
    public static boolean shouldShowSettingsToggle(Context context) {
        return isBottomBarEligible(context)
                && ChromeFeatureList.sAndroidBottomBarShowSettingsToggle.getValue();
    }

    /**
     * Returns whether the activity should observe the user's bottom bar choice and recreate itself
     * when it changes: the Settings switch or the education opt-in dialog can flip it. The bottom
     * bar must be eligible on this device, because the observer only compares the user's choice and
     * would otherwise keep recreating the activity on tablets, unfolded foldables and automotive
     * devices.
     */
    public static boolean shouldObserveBottomBarEnabledChanges(Context context) {
        return shouldShowSettingsToggle(context)
                || (isEducationEnabled() && isBottomBarEligible(context));
    }

    /**
     * Returns whether the Address bar and Homepage settings rows should be moved from MainSettings
     * into Appearance settings and Toolbar shortcut renamed to Address bar shortcut: the
     * AndroidBottomBar flag and move_toolbar_settings_to_appearance param must be on, and the
     * device must be a phone or foldable (not automotive or a non-foldable tablet).
     */
    public static boolean shouldMoveToolbarSettingsToAppearance(Context context) {
        return ChromeFeatureList.sAndroidBottomBar.isEnabled()
                && !DeviceInfo.isAutomotive()
                && (DeviceInfo.isFoldable()
                        || !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context))
                && ChromeFeatureList.sAndroidBottomBarMoveToolbarSettingsToAppearance.getValue();
    }

    /**
     * Returns whether the bottom bar is enabled at the user-setting layer, resolved in order:
     *
     * <ol>
     *   <li>The user's explicit choice in {@code BOTTOM_BAR_ENABLED} (from Settings or education).
     *   <li>When {@code use_pinned_default_user_enabled} is true, the in-process value from before
     *       a mid-run pin release, or the saved {@code PENDING_*} / {@code KEPT_*} {@link
     *       DefaultPinState}.
     *   <li>The {@code default_user_enabled} feature param.
     * </ol>
     *
     * <p>Feature consumers should call {@link #isBottomBarEnabled(Context)} instead.
     */
    public static boolean isBottomBarUserEnabled() {
        boolean defaultValue = ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.getValue();
        if (ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.getValue()) {
            defaultValue =
                    sReleasedPinForProcess == TriState.NOT_SET
                            ? getPinnedDefault(defaultValue)
                            : sReleasedPinForProcess == TriState.TRUE;
        }
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.BOTTOM_BAR_ENABLED, defaultValue);
    }

    /**
     * Saves {@code default_user_enabled} as a pending pin on the first start with pinning, unless
     * the user already chose, and records the saved default this run started with. Reads the cached
     * pinning and education params first, so this run keeps them after {@code
     * ChromeCachedFlags#cacheNativeFlags()} refreshes the cache.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    public static void maybePinDefaultUserEnabled() {
        if (!ChromeFeatureList.sAndroidBottomBar.isEnabled()) return;
        // Read cached params now so non-tabbed starts (for example Custom Tabs) and unfolded
        // foldables do not mix pre-native and post-cacheNativeFlags() seeds in the same process.
        boolean defaultValue = ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.getValue();
        boolean pinDefault = ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.getValue();
        ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.getValue();
        ChromeFeatureList.sAndroidBottomBarEducationEnabled.getValue();
        // Record the state carried over from earlier runs. A default saved on this start shows up
        // in the Transition histogram instead, and users who chose in Settings or education are
        // skipped because the saved default does not apply to them.
        @DefaultPinState int state = getDefaultPinState();
        boolean userChose = hasUserSetBottomBarEnabled();
        if (!userChose) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.BottomBar.DefaultPinState.OnStartup", state, DefaultPinState.COUNT);
        }
        if (state != DefaultPinState.NONE || userChose || !pinDefault) {
            return;
        }
        setDefaultPinState(toPendingPinState(defaultValue));
        recordTransition(
                defaultValue ? DefaultPinTransition.PINNED_ON : DefaultPinTransition.PINNED_OFF);
    }

    /**
     * Marks the bottom bar education as shown and promotes the currently active default to {@code
     * DefaultPinState.KEPT_*} when pinning is enabled and the user has no explicit setting.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    static void recordEducationShown() {
        if (!ChromeFeatureList.sAndroidBottomBar.isEnabled()
                || !ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.getValue()
                || hasUserSetBottomBarEnabled()) {
            return;
        }
        // Read isBottomBarUserEnabled() rather than getDefaultPinState() so we keep the state
        // actually shown (for example when use_pinned_default_user_enabled is false).
        boolean shown = isBottomBarUserEnabled();
        setDefaultPinState(toKeptPinState(shown));
        recordTransition(
                shown
                        ? DefaultPinTransition.KEPT_ON_ON_EDUCATION
                        : DefaultPinTransition.KEPT_OFF_ON_EDUCATION);
    }

    /**
     * Returns whether {@code BOTTOM_BAR_DEFAULT_PIN_STATE} is pending its one-time Feature
     * Engagement Tracker backfill.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    static boolean shouldBackfillEducationSeen() {
        @DefaultPinState int state = getDefaultPinState();
        return state == DefaultPinState.PENDING_OFF || state == DefaultPinState.PENDING_ON;
    }

    /**
     * Records the one-time Feature Engagement backfill for a pending pin state. Promotes a pending
     * pin to {@code KEPT_*} when {@code seen} is true. Otherwise writes {@code RELEASED} so the
     * user follows {@code default_user_enabled}, keeping the startup value in memory for this
     * process if releasing it would flip the bar mid-run.
     */
    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    static void backfillEducationSeen(boolean seen) {
        assert shouldBackfillEducationSeen();
        if (seen) {
            boolean pendingOn = getDefaultPinState() == DefaultPinState.PENDING_ON;
            setDefaultPinState(toKeptPinState(pendingOn));
            recordTransition(
                    pendingOn
                            ? DefaultPinTransition.KEPT_ON_ON_BACKFILL
                            : DefaultPinTransition.KEPT_OFF_ON_BACKFILL);
            return;
        }
        boolean shown = isBottomBarUserEnabled();
        setDefaultPinState(DefaultPinState.RELEASED);
        boolean held = isBottomBarUserEnabled() != shown;
        if (held) {
            sReleasedPinForProcess = TriStateUtils.from(shown);
            ResettersForTesting.register(() -> sReleasedPinForProcess = TriState.NOT_SET);
        }
        recordTransition(
                held ? DefaultPinTransition.RELEASED_AND_HELD : DefaultPinTransition.RELEASED);
    }

    private static @DefaultPinState int getDefaultPinState() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, DefaultPinState.NONE);
    }

    private static void setDefaultPinState(@DefaultPinState int state) {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, state);
    }

    // TODO(crbug.com/582747594): Remove once AndroidBottomBar is enabled by default.
    private static void recordTransition(@DefaultPinTransition int transition) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BottomBar.DefaultPinState.Transition",
                transition,
                DefaultPinTransition.COUNT);
    }

    private static boolean getPinnedDefault(boolean fallback) {
        switch (getDefaultPinState()) {
            case DefaultPinState.PENDING_OFF:
            case DefaultPinState.KEPT_OFF:
                return false;
            case DefaultPinState.PENDING_ON:
            case DefaultPinState.KEPT_ON:
                return true;
            case DefaultPinState.NONE:
            case DefaultPinState.RELEASED:
            default:
                return fallback;
        }
    }

    private static @DefaultPinState int toPendingPinState(boolean enabled) {
        return enabled ? DefaultPinState.PENDING_ON : DefaultPinState.PENDING_OFF;
    }

    private static @DefaultPinState int toKeptPinState(boolean enabled) {
        return enabled ? DefaultPinState.KEPT_ON : DefaultPinState.KEPT_OFF;
    }

    /**
     * Returns whether the user has explicitly set the bottom bar enabled preference in Settings.
     */
    public static boolean hasUserSetBottomBarEnabled() {
        return ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
    }

    /**
     * Sets whether the user has the bottom bar turned on in Settings. Because the bottom bar
     * container stub is only inflated during startup (see {@code
     * ToolbarManager#enableBottomControls()}), toggling this setting requires recreating {@code
     * ChromeTabbedActivity} (e.g. via {@code Activity#recreate()}) to take effect.
     */
    public static void setBottomBarUserEnabled(boolean enabled) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_ENABLED, enabled);
    }

    /**
     * Returns the configured height of the bottom bar in DP (clamped to [{@link
     * #MIN_BOTTOM_BAR_HEIGHT_DP}, {@link #MAX_BOTTOM_BAR_HEIGHT_DP}]).
     */
    public static int getBottomBarHeightDp() {
        int heightDp = ChromeFeatureList.sAndroidBottomBarHeightDp.getValue();
        if (heightDp <= 0) return DEFAULT_BOTTOM_BAR_HEIGHT_DP;
        return Math.clamp(heightDp, MIN_BOTTOM_BAR_HEIGHT_DP, MAX_BOTTOM_BAR_HEIGHT_DP);
    }

    /** Whether to include the app menu button in the bottom bar if the flag is enabled. */
    public static boolean shouldIncludeAppMenuButton() {
        return !ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
    }

    /** Whether to show the update badge in the bottom bar app menu button. */
    public static boolean shouldShowAppMenuUpdateBadge() {
        return ChromeFeatureList.sAndroidBottomBarShowUpdateBadge.getValue();
    }

    /** Whether to show the bottom bar on GTS if the flag is enabled. */
    public static boolean shouldShowOnGts() {
        return ChromeFeatureList.sAndroidBottomBarShowBottomBarOnGts.getValue();
    }

    /** Whether to remove top padding in the bottom bar when the bottom omnibox is present. */
    public static boolean shouldRemoveTopPaddingForBottomOmnibox() {
        return ChromeFeatureList.sAndroidBottomBarRemoveTopPaddingForBottomOmnibox.getValue();
    }

    /**
     * Whether bottom controls scroll-off is enabled for the given tab. Scroll-off is enabled for
     * regular (non-incognito) NTP when the bottom bar is enabled.
     */
    public static boolean isNtpScrollOffEnabled(@Nullable Tab tab, @Nullable Context context) {
        if (tab == null || context == null) return false;
        return !tab.isOffTheRecord()
                && isNtp(tab)
                && isBottomBarEnabled(context)
                && ChromeFeatureList.sAndroidBottomBarNtpScrollOffEnabled.getValue();
    }

    /**
     * Whether to force {@link BrowserControlsState#BOTH} constraints for the bottom controls.
     *
     * <p>When the current tab is on an NTP or other native page, the constraints emitted for the
     * bottom bar are overridden and forced to {@link BrowserControlsState#BOTH}. This ensures that
     * ScrollingBottomViewResourceFrameLayout allows screenshot updates, preventing stale
     * screenshots. It does not affect the physical scroll behavior of the bottom bar, which is
     * driven by the actual tab constraints.
     */
    public static boolean shouldForceBothConstraintsForBottomControls(
            @Nullable Tab tab, @Nullable Context context) {
        if (tab == null || context == null || !isBottomBarEnabled(context)) return false;

        return tab.isNativePage() || UrlUtilities.isInternalScheme(tab.getUrl());
    }

    /** Whether to always use the filled GLIC icon. */
    public static boolean alwaysUseFilledIcon() {
        return ChromeFeatureList.sAndroidBottomBarAlwaysUseFilledGlicIcon.getValue();
    }

    /**
     * Whether the given tab is showing any NTP (regular or incognito).
     *
     * <p>Keys off the native page rather than the URL: the visible URL changes when a navigation
     * starts, but the native page is only swapped when it commits. Frozen tabs keep a
     * FrozenNativePage with the original host.
     */
    public static boolean isNtp(@Nullable Tab tab) {
        NativePage nativePage = tab == null ? null : tab.getNativePage();
        return nativePage != null && UrlConstants.NTP_HOST.equals(nativePage.getHost());
    }

    /** Whether the feature parameter to show the GLIC setting toggle is enabled. */
    public static boolean isGlicSettingToggleParamEnabled() {
        return ChromeFeatureList.sAndroidBottomBarShowGlicSettingToggle.getValue();
    }

    /** Returns whether the GLIC button is enabled by the user in the bottom bar. */
    public static boolean isGlicButtonEnabled() {
        if (!isGlicSettingToggleParamEnabled()) return true;
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED, true);
    }

    /** Sets whether the GLIC button is enabled in the bottom bar. */
    public static void setGlicButtonEnabled(boolean enabled) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED, enabled);
    }

    /**
     * Returns whether the bottom bar user education flow is enabled. False while AndroidBottomBar
     * is off, so a temporary rollback or safe mode does not release pending saved defaults.
     * Suppressed for the rest of a process when a backfill released a saved default that differed
     * from the new default, so education does not describe a bar state that only takes effect on
     * the next start.
     */
    public static boolean isEducationEnabled() {
        // TODO(crbug.com/582747594): Remove sReleasedPinForProcess once AndroidBottomBar is
        // enabled by default.
        return ChromeFeatureList.sAndroidBottomBar.isEnabled()
                && sReleasedPinForProcess == TriState.NOT_SET
                && ChromeFeatureList.sAndroidBottomBarEducationEnabled.getValue();
    }

    /**
     * Returns whether long-pressing empty space on the bottom bar opens its menu. A kill switch:
     * when off, the bar is not long-clickable at all.
     */
    public static boolean isLongPressMenuEnabled() {
        return ChromeFeatureList.sAndroidBottomBarLongPressMenuEnabled.getValue();
    }

    /**
     * Returns whether the user is eligible for the education opt-in dialog: education is enabled,
     * the device is eligible for the bottom bar, the bar is off by default, and the user hasn't
     * explicitly turned it on or off.
     */
    public static boolean isEligibleForEducationDialog(Context context) {
        return isEducationEnabled()
                && isBottomBarEligible(context)
                && !isBottomBarUserEnabled()
                && !hasUserSetBottomBarEnabled();
    }

    /**
     * Returns whether the bottom bar user education flow should show the address bar position IPH.
     */
    public static boolean isEducationAddressBarIphEnabled() {
        return isEducationEnabled()
                && ChromeFeatureList.sAndroidBottomBarEducationAddressBarIphEnabled.getValue();
    }

    /** Whether the bottom bar user education flow should use the Gemini-focused variant. */
    public static boolean isEducationGeminiFocused() {
        return ChromeFeatureList.sAndroidBottomBarEducationGeminiFocused.getValue();
    }
}
