// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.content.SharedPreferences;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;

import com.google.android.gms.location.Priority;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.List;

/** This is the place where we define these: List of Omnibox features and parameters. */
@NullMarked
public class OmniboxFeatures {
    @IntDef({FeatureState.DISABLED, FeatureState.ENABLED_IN_TEST, FeatureState.ENABLED_IN_PROD})
    @Retention(RetentionPolicy.SOURCE)
    @interface FeatureState {
        int DISABLED = 0;
        int ENABLED_IN_TEST = 1;
        int ENABLED_IN_PROD = 2;
    }

    // LINT.IfChange(OmniboxJumpStartState)
    // TODO(ender): move OmniboxMetrics to //components and relocate this code there.
    @IntDef({
        OmniboxJumpStartState.NOT_ELIGIBLE,
        OmniboxJumpStartState.ENABLED,
        OmniboxJumpStartState.DISABLED_BY_USER,
        OmniboxJumpStartState.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface OmniboxJumpStartState {
        int NOT_ELIGIBLE = 0;
        int ENABLED = 1;
        int DISABLED_BY_USER = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:OmniboxJumpStartState)

    private static final SharedPreferences sPrefs = ContextUtils.getAppSharedPreferences();

    /** The state of the Jump Start Omnibox feature. */
    public static final String KEY_JUMP_START_OMNIBOX = "jump_start_omnibox";

    /** The timestamp representing the last time the user exited Chrome. */
    public static final String KEY_LAST_EXIT_TIMESTAMP = "last_exit_timestamp";

    // Threshold for low RAM devices. We won't be showing suggestion images
    // on devices that have less RAM than this to avoid bloat and reduce user-visible
    // slowdown while spinning up an image decompression process.
    // We set the threshold to 1.5GB to reduce number of users affected by this restriction.
    private static final int LOW_MEMORY_THRESHOLD_KB = (int) (1.5 * 1024 * 1024);

    // Maximum number of attempts to retrieve page behind the default match per Omnibox input
    // session.
    public static final int DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION = 5;

    // Timeout requests after 30 minutes if we somehow fail to remove our listener.
    private static final int DEFAULT_GEOLOCATION_REQUEST_TIMEOUT_MIN = 10;

    // Minimum number of characters required to trigger rich inline autocomplete.
    private static final int DEFAULT_RICH_INLINE_MIN_CHAR = 3;

    // Auto-populated list of Omnibox cached feature flags.
    // Each flag created via newFlag() will be automatically added to this list.
    private static final List<CachedFlag> sCachedFlags = new ArrayList<>();
    private static final List<CachedFeatureParam<?>> sCachedParams = new ArrayList<>();

    /// Holds the information whether logic should focus on preserving memory on this device.
    private static @Nullable Boolean sIsLowMemoryDevice;

    public static final CachedFlag sAnimateSuggestionsListAppearance =
            newFlag(
                    OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE,
                    FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sTouchDownTriggerForPrefetch =
            newFlag(
                    OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH,
                    FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sUrlBarWithoutLigatures =
            newFlag(OmniboxFeatureList.URL_BAR_WITHOUT_LIGATURES, FeatureState.ENABLED_IN_PROD);

    /**
     * Whether GeolocationHeader should use {@link
     * com.google.android.gms.location.FusedLocationProviderClient} to determine the location sent
     * in omnibox requests.
     */
    public static final CachedFlag sUseFusedLocationProvider =
            newFlag(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sAsyncViewInflation =
            newFlag(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sJumpStartOmnibox =
            newFlag(OmniboxFeatureList.JUMP_START_OMNIBOX, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sPostDelayedTaskFocusTab =
            newFlag(OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sOmniboxMobileParityUpdateV2 =
            newFlag(
                    OmniboxFeatureList.OMNIBOX_MOBILE_PARITY_UPDATE_V2,
                    FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sOmniboxSiteSearch =
            newFlag(OmniboxFeatureList.OMNIBOX_SITE_SEARCH, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sOmniboxMultimodalInput =
            newFlag(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT, FeatureState.ENABLED_IN_TEST);

    public static final BooleanCachedFeatureParam sShowDedicatedModeButton =
            newBooleanParam(sOmniboxMultimodalInput, "show_dedicated_mode_button", false);

    public static final BooleanCachedFeatureParam sShowTryAiModeHintInDedicatedModeButton =
            newBooleanParam(sOmniboxMultimodalInput, "show_try_aimode_hint_in_mode_button", false);

    public static final BooleanCachedFeatureParam sShowImageGenerationButtonInIncognito =
            newBooleanParam(sOmniboxMultimodalInput, "show_image_gen_button_in_incognito", true);

    public static final BooleanCachedFeatureParam sCompactFusebox =
            newBooleanParam(sOmniboxMultimodalInput, "compact_fusebox", false);

    public static final BooleanCachedFeatureParam sMultiattachmentFusebox =
            newBooleanParam(sOmniboxMultimodalInput, "multi_context", false);

    public static final CachedFlag sMultilineEditField =
            newFlag(OmniboxFeatureList.MULTILINE_EDIT_FIELD, FeatureState.ENABLED_IN_TEST);

    public static final BooleanCachedFeatureParam sWrapAutocompleteText =
            newBooleanParam(sOmniboxMultimodalInput, "wrap_autocomplete_text", false);

    public static final CachedFlag sAndroidHubSearchTabGroups =
            newFlag(OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sOmniboxImprovementForLFF =
            newFlag(OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF, FeatureState.DISABLED);

    public static final CachedFlag sRemoveSearchReadyOmnibox =
            newFlag(OmniboxFeatureList.REMOVE_SEARCH_READY_OMNIBOX, FeatureState.ENABLED_IN_TEST);

    public static final BooleanCachedFeatureParam sRemoveSroIncludingVerbatimMatch =
            newBooleanParam(
                    sRemoveSearchReadyOmnibox, "remove_sro_including_verbatim_match", false);

    public static final BooleanCachedFeatureParam sOmniboxParityRetrieveBuiltInEngineIcon =
            newBooleanParam(sOmniboxMobileParityUpdateV2, "retrieve_builtin_favicon", true);

    public static final IntCachedFeatureParam sGeolocationRequestTimeoutMinutes =
            newIntParam(
                    sUseFusedLocationProvider,
                    "geolocation_request_timeout_minutes",
                    DEFAULT_GEOLOCATION_REQUEST_TIMEOUT_MIN);

    public static final IntCachedFeatureParam sGeolocationRequestMaxLocationAge =
            newIntParam(
                    sUseFusedLocationProvider,
                    "geolocation_request_max_location_age_millis",
                    (int) (5 * DateUtils.MINUTE_IN_MILLIS));

    public static final IntCachedFeatureParam sGeolocationRequestUpdateInterval =
            newIntParam(
                    sUseFusedLocationProvider,
                    "geolocation_request_min_update_interval_millis",
                    (int) (9 * DateUtils.MINUTE_IN_MILLIS));

    public static final IntCachedFeatureParam sGeolocationRequestPriority =
            newIntParam(
                    sUseFusedLocationProvider,
                    "geolocation_request_priority",
                    Priority.PRIORITY_BALANCED_POWER_ACCURACY);

    public static final IntCachedFeatureParam sTouchDownTriggerMaxPrefetchesPerSession =
            newIntParam(
                    sTouchDownTriggerForPrefetch,
                    "max_prefetches_per_omnibox_session",
                    DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION);

    public static final IntCachedFeatureParam sJumpStartOmniboxMemoryThresholdKb =
            newIntParam(sJumpStartOmnibox, "jump_start_memory_threshold_kb", 2 * 1024 * 1024);

    public static final IntCachedFeatureParam sJumpStartOmniboxMinAwayTimeMinutes =
            newIntParam(sJumpStartOmnibox, "jump_start_min_away_time_minutes", 0 * 60);

    public static final IntCachedFeatureParam sJumpStartOmniboxMaxAwayTimeMinutes =
            newIntParam(sJumpStartOmnibox, "jump_start_max_away_time_minutes", 8 * 60);

    public static final IntCachedFeatureParam sPostDelayedTaskFocusTabTimeMillis =
            newIntParam(sPostDelayedTaskFocusTab, "post_delayed_task_focus_tab_time_millis", 0);

    // This parameter permits JSO to include additional page classifications when caching/serving
    // suggestions on SearchActivity.
    public static final BooleanCachedFeatureParam sJumpStartOmniboxCoverRecentlyVisitedPage =
            newBooleanParam(sJumpStartOmnibox, "jump_start_cover_recently_visited_page", false);

    // This parameter enables the hub search entrypoints on the tab groups pane.
    public static final BooleanCachedFeatureParam sAndroidHubSearchEnableOnTabGroupsPane =
            newBooleanParam(sAndroidHubSearchTabGroups, "enable_hub_search_tab_groups_pane", true);

    // This parameter enables the tab group string on the hub search box entrypoint.
    public static final BooleanCachedFeatureParam sAndroidHubSearchEnableTabGroupStrings =
            newBooleanParam(
                    sAndroidHubSearchTabGroups, "enable_hub_search_tab_group_strings", false);

    // This parameter enables showing the switch-to-tab chip on large form factors.
    public static final BooleanCachedFeatureParam sOmniboxImprovementForLFFSwitchToTabChip =
            newBooleanParam(sOmniboxImprovementForLFF, "switch_to_tab_chip", false);

    // This parameter enables removing suggestion via "x" button.
    public static final BooleanCachedFeatureParam
            sOmniboxImprovementForLFFRemoveSuggestionViaButton =
                    newBooleanParam(
                            sOmniboxImprovementForLFF, "remove_suggestion_via_button", false);

    // This parameter enables persisting editing state.
    public static final BooleanCachedFeatureParam sOmniboxImprovementForLFFPersistEditingState =
            newBooleanParam(sOmniboxImprovementForLFF, "persist_editing_state", false);

    // Omnibox Diagnostics
    private static final CachedFlag sDiagnostics =
            newFlag(OmniboxFeatureList.DIAGNOSTICS, FeatureState.DISABLED);
    public static final BooleanCachedFeatureParam sDiagInputConnection =
            newBooleanParam(sDiagnostics, "omnibox_diag_input_connection", false);

    /** See {@link #setShouldRetainOmniboxOnFocusForTesting(boolean)}. */
    private static @Nullable Boolean sShouldRetainOmniboxOnFocusForTesting;

    /** When enabled, Jump Start Omnibox is activated and can engage if the feature is enabled. */
    private static @Nullable Boolean sActivateJumpStartOmnibox;

    /**
     * Create an instance of a CachedFeatureFlag.
     *
     * @param featureName the name of the feature flag
     * @param state the state of the feature flag
     */
    private static CachedFlag newFlag(String featureName, @FeatureState int state) {
        var cachedFlag =
                new CachedFlag(
                        OmniboxFeatureMap.getInstance(),
                        featureName,
                        /* defaultValue= */ state == FeatureState.ENABLED_IN_PROD,
                        /* defaultValueInTests= */ state != FeatureState.DISABLED);
        sCachedFlags.add(cachedFlag);
        return cachedFlag;
    }

    /**
     * Create an instance of a BooleanCachedFeatureParam.
     *
     * <p>Newly created flag will be automatically added to list of persisted feature flags.
     *
     * @param flag the Feature flag the parameter is associated with
     * @param variationName the name of the associated parameter
     * @param defaultValue the default value to return if the feature state is unknown
     */
    private static BooleanCachedFeatureParam newBooleanParam(
            CachedFlag flag, String variationName, boolean defaultValue) {
        var param =
                new BooleanCachedFeatureParam(
                        OmniboxFeatureMap.getInstance(),
                        flag.getFeatureName(),
                        variationName,
                        defaultValue);
        sCachedParams.add(param);
        return param;
    }

    /**
     * Create an instance of a IntCachedFeatureParam.
     *
     * <p>Newly created flag will be automatically added to list of persisted feature flags.
     *
     * @param flag the Feature flag the parameter is associated with
     * @param variationName the name of the associated parameter
     * @param defaultValue the default value to return if the feature state is unknown
     */
    private static IntCachedFeatureParam newIntParam(
            CachedFlag flag, String variationName, int defaultValue) {
        var param =
                new IntCachedFeatureParam(
                        OmniboxFeatureMap.getInstance(),
                        flag.getFeatureName(),
                        variationName,
                        defaultValue);
        sCachedParams.add(param);
        return param;
    }

    /** Retrieve list of CachedFlags that should be cached. */
    public static List<CachedFlag> getFlagsToCache() {
        return sCachedFlags;
    }

    /** Retrieve list of FeatureParams that should be cached. */
    public static List<CachedFeatureParam<?>> getFeatureParamsToCache() {
        return sCachedParams;
    }

    /**
     * Returns whether the omnibox's recycler view pool should be pre-warmed prior to initial use.
     */
    public static boolean shouldPreWarmRecyclerViewPool() {
        return !isLowMemoryDevice();
    }

    /**
     * Returns whether the device is to be considered low-end for any memory intensive operations.
     */
    public static boolean isLowMemoryDevice() {
        if (sIsLowMemoryDevice == null) {
            sIsLowMemoryDevice =
                    (SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB
                            && !CommandLine.getInstance()
                                    .hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE));
        }
        return sIsLowMemoryDevice;
    }

    /**
     * Returns whether a touch down event on a search suggestion should send a signal to prefetch
     * the corresponding page.
     */
    public static boolean isTouchDownTriggerForPrefetchEnabled() {
        return sTouchDownTriggerForPrefetch.isEnabled();
    }

    /**
     * Returns the maximum number of prefetches that can be triggered by touch down events within an
     * omnibox session.
     */
    public static int getMaxPrefetchesPerOmniboxSession() {
        return sTouchDownTriggerMaxPrefetchesPerSession.getValue();
    }

    /**
     * Whether the appearance of the omnibox suggestions list should animated in sync with the soft
     * keyboard.
     */
    public static boolean shouldAnimateSuggestionsListAppearance() {
        return sAnimateSuggestionsListAppearance.isEnabled();
    }

    /** Indicate a low memory device for testing purposes. */
    public static void setIsLowMemoryDeviceForTesting(boolean isLowMemDevice) {
        sIsLowMemoryDevice = isLowMemDevice;
        ResettersForTesting.register(() -> sIsLowMemoryDevice = null);
    }

    /**
     * Returns whether the rich inline autocomplete URL should be shown.
     *
     * @param inputCount the count of characters user input.
     * @return Whether the rich inline autocomplete URL should be shown.
     */
    public static boolean shouldShowRichInlineAutocompleteUrl(int inputCount) {
        return inputCount >= DEFAULT_RICH_INLINE_MIN_CHAR;
    }

    /** Modifies the output of {@link #shouldRetainOmniboxOnFocus()} for testing. */
    public static void setShouldRetainOmniboxOnFocusForTesting(Boolean shouldRetainOmniboxOnFocus) {
        sShouldRetainOmniboxOnFocusForTesting = shouldRetainOmniboxOnFocus;
        ResettersForTesting.register(() -> sShouldRetainOmniboxOnFocusForTesting = null);
    }

    /**
     * @return Whether the contents of the omnibox should be retained on focus as opposed to being
     *     cleared. When {@code true} and the omnibox contents are retained, focus events will also
     *     result in the omnibox contents being fully selected so as to allow for easy replacement
     *     by the user. Note that only large screen devices with an attached keyboard and precision
     *     pointer will exhibit a change in behavior when the feature flag is enabled.
     */
    public static boolean shouldRetainOmniboxOnFocus() {
        if (sShouldRetainOmniboxOnFocusForTesting != null) {
            return sShouldRetainOmniboxOnFocusForTesting;
        }
        return DeviceFormFactor.isTablet()
                && DeviceInput.supportsAlphabeticKeyboard()
                && DeviceInput.supportsPrecisionPointer();
    }

    /**
     * Returns true if Jump-Start Omnibox should engage, redirecting the user to the SearchActivity.
     */
    public static boolean shouldJumpStartOmniboxEngage() {
        long elapsedTimeSinceLastExit = getTimeSinceLastExit();
        return isJumpStartOmniboxEnabled()
                && (elapsedTimeSinceLastExit
                        >= sJumpStartOmniboxMinAwayTimeMinutes.getValue()
                                * TimeUtils.MILLISECONDS_PER_MINUTE)
                && (elapsedTimeSinceLastExit
                        < sJumpStartOmniboxMaxAwayTimeMinutes.getValue()
                                * TimeUtils.MILLISECONDS_PER_MINUTE);
    }

    /** Returns the cached value of the Jump-Start settings toggle. */
    public static boolean isJumpStartOmniboxEnabled() {
        if (!OmniboxFeatures.sJumpStartOmnibox.isEnabled()) return false;
        if (sActivateJumpStartOmnibox == null) {
            boolean isEligibleDevice =
                    !DeviceFormFactor.isTablet()
                            && SysUtils.amountOfPhysicalMemoryKB()
                                    <= sJumpStartOmniboxMemoryThresholdKb.getValue();
            sActivateJumpStartOmnibox = sPrefs.getBoolean(KEY_JUMP_START_OMNIBOX, isEligibleDevice);
            @OmniboxJumpStartState
            int state =
                    isEligibleDevice
                            ? sActivateJumpStartOmnibox
                                    ? OmniboxJumpStartState.ENABLED // Eligible and activated
                                    : OmniboxJumpStartState.DISABLED_BY_USER // Eligible only
                            : OmniboxJumpStartState.NOT_ELIGIBLE; // Not eligible.
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.Omnibox.JumpStartState", state, OmniboxJumpStartState.COUNT);
        }
        return sActivateJumpStartOmnibox;
    }

    /** Updates the cached value of the Jump-Start settings toggle. */
    public static void setJumpStartOmniboxEnabled(boolean isEnabled) {
        assert OmniboxFeatures.sJumpStartOmnibox.isEnabled();
        sActivateJumpStartOmnibox = isEnabled;
        sPrefs.edit().putBoolean(KEY_JUMP_START_OMNIBOX, isEnabled).apply();
    }

    /** Returns the time elapsed since the user exited Chrome, expressed in milliseconds. */
    public static long getTimeSinceLastExit() {
        return TimeUtils.currentTimeMillis() - sPrefs.getLong(KEY_LAST_EXIT_TIMESTAMP, 0L);
    }

    /** Record the current time as the time the User exited Chrome. */
    public static void updateLastExitTimestamp() {
        sPrefs.edit().putLong(KEY_LAST_EXIT_TIMESTAMP, TimeUtils.currentTimeMillis()).apply();
    }

    public static boolean allowMultilineEditField() {
        return (!DeviceFormFactor.isTablet() && sMultilineEditField.isEnabled());
    }

    public static boolean shouldJumpStartOmnibox() {
        return isJumpStartOmniboxEnabled();
    }
}
