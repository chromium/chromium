// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils.DefaultPinState;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils.DefaultPinTransition;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarConfigUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp")
public class BottomBarConfigUtilsUnitTest {
    private static final String ON_STARTUP = "Android.BottomBar.DefaultPinState.OnStartup";
    private static final String TRANSITION = "Android.BottomBar.DefaultPinState.Transition";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;

    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Tablet() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Phone() {
        assertTrue(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Automotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarDisabled() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_UserDisabled() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_UserEnabledFlagDisabled() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEligible_Phone_IgnoresUserPref() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertTrue(BottomBarConfigUtils.isBottomBarEligible(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEligible_Tablet() {
        assertFalse(BottomBarConfigUtils.isBottomBarEligible(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEligible_Automotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.isBottomBarEligible(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEligible_FlagDisabled() {
        assertFalse(BottomBarConfigUtils.isBottomBarEligible(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/true")
    public void testIsBottomBarUserEnabled_PrefUnset_DefaultParamTrue() {
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarUserEnabled_PrefUnset_DefaultParamFalse() {
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarUserEnabled_UserChoiceWinsOverDefaultParamFalse() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/true")
    public void testIsBottomBarUserEnabled_UserChoiceWinsOverDefaultParamTrue() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarEnabled_DefaultParamFalse_PrefUnset() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testSavingDefault_SkippedWhenFlagOffPinningOffOrUserChose() {
        Runnable[] writers = {
            BottomBarConfigUtils::maybePinDefaultUserEnabled,
            BottomBarConfigUtils::recordEducationShown
        };
        for (Runnable writer : writers) {
            FeatureOverrides.disable(ChromeFeatureList.ANDROID_BOTTOM_BAR);
            writer.run();
            assertFalse(hasPinState());
            FeatureOverrides.enable(ChromeFeatureList.ANDROID_BOTTOM_BAR);

            // The launch config turns pinning off.
            ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.setForTesting(false);
            writer.run();
            assertFalse(hasPinState());
            ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.setForTesting(true);

            BottomBarConfigUtils.setBottomBarUserEnabled(false);
            writer.run();
            assertFalse(hasPinState());
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybePinDefaultUserEnabled_SavesDefaultOnce() {
        for (boolean armDefault : new boolean[] {false, true}) {
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE);
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(armDefault);
            BottomBarConfigUtils.maybePinDefaultUserEnabled();
            assertEquals(pendingState(armDefault), readPinState());

            // Later starts with the other default leave a pending, kept or released state alone.
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(!armDefault);
            int[] savedStates = {
                pendingState(armDefault), keptState(armDefault), DefaultPinState.RELEASED
            };
            for (@DefaultPinState int state : savedStates) {
                writePinState(state);
                BottomBarConfigUtils.maybePinDefaultUserEnabled();
                assertEquals(state, readPinState());
            }
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarUserEnabled_FollowsSavedDefault() {
        for (boolean armDefault : new boolean[] {false, true}) {
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(!armDefault);
            int[] savedStates = {pendingState(armDefault), keptState(armDefault)};
            for (@DefaultPinState int state : savedStates) {
                writePinState(state);
                assertEquals(armDefault, BottomBarConfigUtils.isBottomBarUserEnabled());
                // Saving a default does not count as an explicit user setting, so the opt-in
                // dialog still shows while the bar is off.
                assertEquals(
                        !armDefault, BottomBarConfigUtils.isEligibleForEducationDialog(mContext));

                ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(false);
                assertEquals(!armDefault, BottomBarConfigUtils.isBottomBarUserEnabled());
                ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(true);
            }

            writePinState(DefaultPinState.RELEASED);
            assertEquals(!armDefault, BottomBarConfigUtils.isBottomBarUserEnabled());
        }
    }

    @Test
    public void testBackfillEducationSeen_Seen_KeepsSavedDefault() {
        for (boolean armDefault : new boolean[] {false, true}) {
            writePinState(pendingState(armDefault));
            BottomBarConfigUtils.backfillEducationSeen(/* seen= */ true);
            assertEquals(keptState(armDefault), readPinState());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true")
    public void testBackfillEducationSeen_NeverSeen_ReleasesWithoutChangingThisRun() {
        for (boolean armDefault : new boolean[] {false, true}) {
            // The saved default matches default_user_enabled.
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(armDefault);
            writePinState(pendingState(armDefault));
            backfillNeverSeenAndAssertThisRunUnchanged();

            // The kill switch already ignores the saved default.
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(!armDefault);
            ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(false);
            writePinState(pendingState(armDefault));
            backfillNeverSeenAndAssertThisRunUnchanged();
            ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(true);

            // An explicit choice wins over the saved default.
            BottomBarConfigUtils.setBottomBarUserEnabled(!armDefault);
            writePinState(pendingState(armDefault));
            backfillNeverSeenAndAssertThisRunUnchanged();
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
        }
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":default_user_enabled/true/education_enabled/true")
    public void testBackfillEducationSeen_NeverSeen_KeepsShownBarUntilNextStart() {
        writePinState(DefaultPinState.PENDING_OFF);
        BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
        assertEquals(DefaultPinState.RELEASED, readPinState());
        // The bar stays off for this run, and the education waits for the next start.
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        assertFalse(BottomBarConfigUtils.isEducationEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testRecordEducationShown_KeepsShownDefault() {
        for (boolean armDefault : new boolean[] {false, true}) {
            // A pending or kept default is what the bar showed, whatever default_user_enabled is.
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(!armDefault);
            int[] savedStates = {pendingState(armDefault), keptState(armDefault)};
            for (@DefaultPinState int state : savedStates) {
                writePinState(state);
                BottomBarConfigUtils.recordEducationShown();
                assertEquals(keptState(armDefault), readPinState());
            }

            // Without a saved default, or when the kill switch ignores it, the bar showed
            // default_user_enabled.
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(armDefault);
            int[] unsavedStates = {DefaultPinState.NONE, DefaultPinState.RELEASED};
            for (@DefaultPinState int state : unsavedStates) {
                writePinState(state);
                BottomBarConfigUtils.recordEducationShown();
                assertEquals(keptState(armDefault), readPinState());
            }
            ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(false);
            writePinState(pendingState(!armDefault));
            BottomBarConfigUtils.recordEducationShown();
            assertEquals(keptState(armDefault), readPinState());
            ChromeFeatureList.sAndroidBottomBarUsePinnedDefaultUserEnabled.setForTesting(true);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testRecordEducationShown_FirstRunOnNewSeed_KeepsShownDefault() {
        SharedPreferencesManager cachedPrefs = CachedFlagsSharedPreferences.getInstance();
        String cachedDefaultKey =
                CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled");
        String cachedPinningKey =
                CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":pin_default_user_enabled");
        String cachedUsePinnedKey =
                CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":use_pinned_default_user_enabled");
        String cachedEducationKey =
                CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled");

        // Simulate a cold start where isBottomBarUserEnabled() has not run before native init
        // (for example Custom Tabs or an unfolded foldable). maybePinDefaultUserEnabled() locks
        // this process's cached bottom bar params before cacheNativeFlags() writes the new seed.
        writePinState(DefaultPinState.PENDING_OFF);
        cachedPrefs.writeBoolean(cachedDefaultKey, false);
        cachedPrefs.writeBoolean(cachedPinningKey, true);
        cachedPrefs.writeBoolean(cachedUsePinnedKey, true);
        cachedPrefs.writeBoolean(cachedEducationKey, true);
        BottomBarConfigUtils.maybePinDefaultUserEnabled();
        cachedPrefs.writeBoolean(cachedDefaultKey, true);
        cachedPrefs.writeBoolean(cachedPinningKey, false);
        cachedPrefs.writeBoolean(cachedUsePinnedKey, false);
        cachedPrefs.writeBoolean(cachedEducationKey, false);

        // When a tabbed activity opens later in the same process, it still uses the pre-launch
        // seed and keeps the default that onboarding showed.
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
        assertEquals(DefaultPinState.RELEASED, readPinState());
        assertTrue(BottomBarConfigUtils.isEducationEnabled());
        BottomBarConfigUtils.recordEducationShown();
        assertEquals(DefaultPinState.KEPT_OFF, readPinState());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybePinDefaultUserEnabled_RecordsStartupStateAndPin() {
        for (boolean armDefault : new boolean[] {false, true}) {
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE);
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(armDefault);

            // The first start with pinning records the state it started with, then saves the arm
            // default.
            HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(ON_STARTUP, DefaultPinState.NONE)
                            .expectIntRecord(
                                    TRANSITION,
                                    armDefault
                                            ? DefaultPinTransition.PINNED_ON
                                            : DefaultPinTransition.PINNED_OFF)
                            .build();
            BottomBarConfigUtils.maybePinDefaultUserEnabled();
            watcher.assertExpected();

            // Later starts only record the state.
            watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(ON_STARTUP, pendingState(armDefault))
                            .expectNoRecords(TRANSITION)
                            .build();
            BottomBarConfigUtils.maybePinDefaultUserEnabled();
            watcher.assertExpected();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybePinDefaultUserEnabled_SkipsHistogramsWhenNotApplicable() {
        // Nothing is recorded while the flag is off.
        FeatureOverrides.disable(ChromeFeatureList.ANDROID_BOTTOM_BAR);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(ON_STARTUP)
                        .expectNoRecords(TRANSITION)
                        .build();
        BottomBarConfigUtils.maybePinDefaultUserEnabled();
        watcher.assertExpected();
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_BOTTOM_BAR);

        // The launch config (pinning off) records the state but saves nothing.
        ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.setForTesting(false);
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ON_STARTUP, DefaultPinState.NONE)
                        .expectNoRecords(TRANSITION)
                        .build();
        BottomBarConfigUtils.maybePinDefaultUserEnabled();
        watcher.assertExpected();
        ChromeFeatureList.sAndroidBottomBarPinDefaultUserEnabled.setForTesting(true);

        // A user who chose in Settings is not counted, whatever pin state they carry.
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        writePinState(DefaultPinState.KEPT_OFF);
        watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(ON_STARTUP)
                        .expectNoRecords(TRANSITION)
                        .build();
        BottomBarConfigUtils.maybePinDefaultUserEnabled();
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testRecordEducationShown_RecordsKeptTransition() {
        for (boolean armDefault : new boolean[] {false, true}) {
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(!armDefault);
            writePinState(pendingState(armDefault));
            HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            TRANSITION,
                            armDefault
                                    ? DefaultPinTransition.KEPT_ON_ON_EDUCATION
                                    : DefaultPinTransition.KEPT_OFF_ON_EDUCATION);
            BottomBarConfigUtils.recordEducationShown();
            watcher.assertExpected();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testBackfillEducationSeen_RecordsTransition() {
        for (boolean armDefault : new boolean[] {false, true}) {
            ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(armDefault);

            // Seen keeps the pending default.
            writePinState(pendingState(armDefault));
            HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            TRANSITION,
                            armDefault
                                    ? DefaultPinTransition.KEPT_ON_ON_BACKFILL
                                    : DefaultPinTransition.KEPT_OFF_ON_BACKFILL);
            BottomBarConfigUtils.backfillEducationSeen(/* seen= */ true);
            watcher.assertExpected();

            // Never seen with the same default releases it without a hold.
            writePinState(pendingState(armDefault));
            watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            TRANSITION, DefaultPinTransition.RELEASED);
            BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
            watcher.assertExpected();
        }

        // A release that would flip the bar is held for this run. Last, since the hold is
        // process-wide until the resetter runs.
        ChromeFeatureList.sAndroidBottomBarDefaultUserEnabled.setForTesting(true);
        writePinState(DefaultPinState.PENDING_OFF);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TRANSITION, DefaultPinTransition.RELEASED_AND_HELD);
        BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
        watcher.assertExpected();
    }

    @Test
    public void testHasUserSetBottomBarEnabled() {
        assertFalse(BottomBarConfigUtils.hasUserSetBottomBarEnabled());
        BottomBarConfigUtils.setBottomBarUserEnabled(/* enabled= */ true);
        assertTrue(BottomBarConfigUtils.hasUserSetBottomBarEnabled());
        BottomBarConfigUtils.setBottomBarUserEnabled(/* enabled= */ false);
        assertTrue(BottomBarConfigUtils.hasUserSetBottomBarEnabled());
    }

    @Test
    public void testSetBottomBarUserEnabled_RoundTrip() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Eligible_ParamTrue() {
        assertTrue(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/false")
    public void testShouldShowSettingsToggle_Eligible_ParamFalse() {
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowSettingsToggle_FlagDisabled_ParamDefaultsTrue() {
        // show_settings_toggle defaults to true; the flag being off must still hide the toggle.
        assertTrue(ChromeFeatureList.sAndroidBottomBarShowSettingsToggle.getValue());
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Tablet_ParamTrue() {
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Automotive_ParamTrue() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_IgnoresUserPref() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertTrue(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":show_settings_toggle/false/education_enabled/true")
    public void testShouldObserveBottomBarEnabledChanges_Eligible_EducationEnabled() {
        assertTrue(BottomBarConfigUtils.shouldObserveBottomBarEnabledChanges(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":show_settings_toggle/false/education_enabled/false")
    public void testShouldObserveBottomBarEnabledChanges_Eligible_BothParamsFalse() {
        assertFalse(BottomBarConfigUtils.shouldObserveBottomBarEnabledChanges(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":show_settings_toggle/false/education_enabled/true")
    public void testShouldObserveBottomBarEnabledChanges_Tablet_EducationEnabled() {
        // The observer only compares the user's choice, which defaults to true, so observing where
        // the bar is not eligible would recreate the activity in a loop.
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
        assertFalse(BottomBarConfigUtils.shouldObserveBottomBarEnabledChanges(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":show_settings_toggle/false/education_enabled/true")
    public void testShouldObserveBottomBarEnabledChanges_Automotive_EducationEnabled() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.shouldObserveBottomBarEnabledChanges(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":move_toolbar_settings_to_appearance/true")
    public void testShouldMoveToolbarSettingsToAppearance_Eligible_ParamTrue() {
        assertTrue(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":move_toolbar_settings_to_appearance/false")
    public void testShouldMoveToolbarSettingsToAppearance_Eligible_ParamFalse() {
        assertFalse(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldMoveToolbarSettingsToAppearance_FlagDisabled_ParamDefaultsTrue() {
        assertTrue(ChromeFeatureList.sAndroidBottomBarMoveToolbarSettingsToAppearance.getValue());
        assertFalse(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":move_toolbar_settings_to_appearance/true")
    public void testShouldMoveToolbarSettingsToAppearance_Tablet_ParamTrue() {
        assertFalse(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":move_toolbar_settings_to_appearance/true")
    public void testShouldMoveToolbarSettingsToAppearance_FoldableUnfolded_ParamTrue() {
        DeviceInfo.setIsFoldableForTesting(true);
        assertTrue(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":move_toolbar_settings_to_appearance/true")
    public void testShouldMoveToolbarSettingsToAppearance_Automotive_ParamTrue() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.shouldMoveToolbarSettingsToAppearance(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false")
    public void testShouldIncludeAppMenuButton_FalseParam() {
        assertTrue(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true")
    public void testShouldIncludeAppMenuButton_TrueParam() {
        assertFalse(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/true")
    public void testShouldShowOnGts_TrueParam() {
        assertTrue(BottomBarConfigUtils.shouldShowOnGts());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/false")
    public void testShouldShowOnGts_FalseParam() {
        assertFalse(BottomBarConfigUtils.shouldShowOnGts());
    }

    @Test
    public void testIsNtpScrollOffEnabled_NullInputs() {
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(null, mContext));
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, null));
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(null, null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_ValidNtp() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_Incognito() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_BottomBarDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":ntp_scroll_off_enabled/true")
    public void testIsNtpScrollOffEnabled_KillSwitchEnabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":ntp_scroll_off_enabled/false")
    public void testIsNtpScrollOffEnabled_KillSwitchDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testAlwaysUseFilledIcon_DefaultTrue() {
        assertTrue(BottomBarConfigUtils.alwaysUseFilledIcon());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":always_use_filled_glic_icon/false")
    public void testAlwaysUseFilledIcon_FalseParam() {
        assertFalse(BottomBarConfigUtils.alwaysUseFilledIcon());
    }

    @Test
    public void testShouldForceBothConstraints_NullInputs() {
        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(null, mContext));
        assertFalse(BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, null));
        assertFalse(BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(null, null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_RegularNtp() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_IncognitoNtp() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        // Should return true for incognito NTP even though scroll-off is disabled.
        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_NormalPage() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);

        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_BottomBarDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_InternalScheme() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicButtonEnabled() {
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());

        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertFalse(BottomBarConfigUtils.isGlicButtonEnabled());

        BottomBarConfigUtils.setGlicButtonEnabled(true);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicButtonEnabled_ToggleShown() {
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertFalse(BottomBarConfigUtils.isGlicButtonEnabled());
        BottomBarConfigUtils.setGlicButtonEnabled(true);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testIsGlicButtonEnabled_ToggleNotShown() {
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicSettingToggleParamEnabled_TrueParam() {
        assertTrue(BottomBarConfigUtils.isGlicSettingToggleParamEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testIsGlicSettingToggleParamEnabled_FalseParam() {
        assertFalse(BottomBarConfigUtils.isGlicSettingToggleParamEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsGlicSettingToggleParamEnabled_DefaultBaseline() {
        assertTrue(BottomBarConfigUtils.isGlicSettingToggleParamEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testGetBottomBarHeightDp_DefaultBaseline() {
        assertEquals(
                BottomBarConfigUtils.DEFAULT_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/60")
    public void testGetBottomBarHeightDp_ToolbarMatch60() {
        assertEquals(60, BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/48")
    public void testGetBottomBarHeightDp_CompactAndroid48() {
        assertEquals(48, BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/44")
    public void testGetBottomBarHeightDp_ClampedBelowMin_Returns48() {
        assertEquals(
                BottomBarConfigUtils.MIN_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/72")
    public void testGetBottomBarHeightDp_ClampedAboveMax_Returns60() {
        assertEquals(
                BottomBarConfigUtils.MAX_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/0")
    public void testGetBottomBarHeightDp_ZeroFallback_ReturnsDefault56() {
        assertEquals(
                BottomBarConfigUtils.DEFAULT_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    public void testIsNtp() {
        assertFalse(BottomBarConfigUtils.isNtp(null));

        when(mTab.getNativePage()).thenReturn(null);
        assertFalse(BottomBarConfigUtils.isNtp(mTab));

        NativePage nativePage = mock(NativePage.class);
        when(nativePage.getHost()).thenReturn(UrlConstants.NTP_HOST);
        when(mTab.getNativePage()).thenReturn(nativePage);

        when(mTab.isOffTheRecord()).thenReturn(false);
        assertTrue(BottomBarConfigUtils.isNtp(mTab));

        when(mTab.isOffTheRecord()).thenReturn(true);
        assertTrue(BottomBarConfigUtils.isNtp(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldRemoveTopPaddingForBottomOmnibox_DefaultTrue() {
        assertTrue(BottomBarConfigUtils.shouldRemoveTopPaddingForBottomOmnibox());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":remove_top_padding_for_bottom_omnibox/false")
    public void testShouldRemoveTopPaddingForBottomOmnibox_FalseParam() {
        assertFalse(BottomBarConfigUtils.shouldRemoveTopPaddingForBottomOmnibox());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":default_user_enabled/false/education_enabled/true")
    public void testIsEligibleForEducationDialog_TrueWhenConditionsMet() {
        assertFalse(BottomBarConfigUtils.hasUserSetBottomBarEnabled());
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        assertTrue(BottomBarConfigUtils.isEligibleForEducationDialog(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":default_user_enabled/false/education_enabled/false")
    public void testIsEligibleForEducationDialog_EducationDisabled() {
        assertFalse(BottomBarConfigUtils.isEligibleForEducationDialog(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":default_user_enabled/false/education_enabled/true")
    public void testIsEligibleForEducationDialog_BottomBarIneligible() {
        assertFalse(BottomBarConfigUtils.isEligibleForEducationDialog(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true")
    public void testIsEligibleForEducationDialog_UserHasSetPref() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isEligibleForEducationDialog(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":default_user_enabled/true/education_enabled/true")
    public void testIsEligibleForEducationDialog_DefaultUserEnabledTrue() {
        assertFalse(BottomBarConfigUtils.hasUserSetBottomBarEnabled());
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
        assertFalse(BottomBarConfigUtils.isEligibleForEducationDialog(mContext));
    }

    private static void backfillNeverSeenAndAssertThisRunUnchanged() {
        boolean shown = BottomBarConfigUtils.isBottomBarUserEnabled();
        BottomBarConfigUtils.backfillEducationSeen(/* seen= */ false);
        assertEquals(DefaultPinState.RELEASED, readPinState());
        assertEquals(shown, BottomBarConfigUtils.isBottomBarUserEnabled());
        assertTrue(BottomBarConfigUtils.isEducationEnabled());
    }

    private static boolean hasPinState() {
        return ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE);
    }

    private static @DefaultPinState int readPinState() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, DefaultPinState.NONE);
    }

    private static void writePinState(@DefaultPinState int state) {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, state);
    }

    private static @DefaultPinState int pendingState(boolean enabled) {
        return enabled ? DefaultPinState.PENDING_ON : DefaultPinState.PENDING_OFF;
    }

    private static @DefaultPinState int keptState(boolean enabled) {
        return enabled ? DefaultPinState.KEPT_ON : DefaultPinState.KEPT_OFF;
    }
}
