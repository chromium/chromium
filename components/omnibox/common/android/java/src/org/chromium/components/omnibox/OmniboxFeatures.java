// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.content.SharedPreferences;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.location.Priority;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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

    public static final CachedFlag sOmniboxAnswerActions =
            newFlag(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sAnimateSuggestionsListAppearance =
            newFlag(
                    OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE,
                    FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sTouchDownTriggerForPrefetch =
            newFlag(
                    OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH,
                    FeatureState.ENABLED_IN_PROD);

    /**
     * Whether GeolocationHeader should use {@link
     * com.google.android.gms.location.FusedLocationProviderClient} to determine the location sent
     * in omnibox requests.
     */
    public static final CachedFlag sUseFusedLocationProvider =
            newFlag(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sAsyncViewInflation =
            newFlag(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sElegantTextHeight =
            newFlag(OmniboxFeatureList.OMNIBOX_ELEGANT_TEXT_HEIGHT, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sJumpStartOmnibox =
            newFlag(OmniboxFeatureList.JUMP_START_OMNIBOX, FeatureState.ENABLED_IN_TEST);

    /** See {@link #shouldRetainOmniboxOnFocus()}. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final CachedFlag sRetainOmniboxOnFocus =
            newFlag(OmniboxFeatureList.RETAIN_OMNIBOX_ON_FOCUS, FeatureState.ENABLED_IN_TEST);

    public static final CachedFlag sAndroidHubSearch =
            newFlag(OmniboxFeatureList.ANDROID_HUB_SEARCH, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sPostDelayedTaskFocusTab =
            newFlag(OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB, FeatureState.ENABLED_IN_PROD);

    public static final CachedFlag sOmniboxMobileParityUpdate =
            newFlag(OmniboxFeatureList.OMNIBOX_MOBILE_PARITY_UPDATE, FeatureState.ENABLED_IN_TEST);

    public static final BooleanCachedFeatureParam sOmniboxParityRetrieveTrueFavicon =
            newBooleanParam(sOmniboxMobileParityUpdate, "retrieve_true_favicon", false);

    public static final BooleanCachedFeatureParam sOmniboxParityEnableFeedOnlyForGoogle =
            newBooleanParam(sOmniboxMobileParityUpdate, "enable_feed_for_google_only", true);

    public static final BooleanCachedFeatureParam sAnswerActionsShowAboveKeyboard =
            newBooleanParam(sOmniboxAnswerActions, "AnswerActionsShowAboveKeyboard", false);

    public static final BooleanCachedFeatureParam sAnswerActionsShowIfUrlsPresent =
            newBooleanParam(sOmniboxAnswerActions, "ShowIfUrlsPresent", false);

    public static final BooleanCachedFeatureParam sAnswerActionsShowRichCard =
            newBooleanParam(sOmniboxAnswerActions, "ShowRichCard", false);

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

    // This parameter allows the user to click enter when on hub search to perform a search on the
    // listed suggestions or perform a google search on the query if no suggestions are found.
    public static final BooleanCachedFeatureParam sAndroidHubSearchEnterPerformsSearch =
            newBooleanParam(sAndroidHubSearch, "enable_press_enter_to_search", true);

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

    /** Returns whether answer suggestions should be annotated with attached action chips. */
    public static boolean shouldShowAnswerActions() {
        return sOmniboxAnswerActions.isEnabled();
    }

    /** Returns whether answers with actions should be re-ordered to just above the keyboard */
    public static boolean shouldShowAnswerWithActionsAboveKeyboard() {
        return shouldShowAnswerActions() && sAnswerActionsShowAboveKeyboard.getValue();
    }

    /**
     * Returns whether answers with actions should be displayed if there are url suggestions
     * present.
     */
    public static boolean shouldShowAnswerWithActionsIfUrlsPresent() {
        return shouldShowAnswerActions() && sAnswerActionsShowIfUrlsPresent.getValue();
    }

    /** Returns whether answers with actions should be presented as a rich card */
    public static boolean shouldShowRichAnswerCard() {
        return shouldShowAnswerActions() && sAnswerActionsShowRichCard.getValue();
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
        return sRetainOmniboxOnFocus.isEnabled()
                && DeviceFormFactor.isTablet()
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

    public static boolean shouldJumpStartOmnibox() {
        return isJumpStartOmniboxEnabled();
    }
}
