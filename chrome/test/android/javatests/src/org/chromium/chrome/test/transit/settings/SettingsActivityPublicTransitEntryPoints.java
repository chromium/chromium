// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;

import java.util.concurrent.Callable;

/** Entry points for Public Transit tests that use SettingsActivity. */
public class SettingsActivityPublicTransitEntryPoints {
    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule;
    private static SettingsActivity sActivity;

    /**
     * Constructs the settings activity entry points for the Public Transit tests.
     *
     * @param settingsActivityTestRule The test rule capable of starting the settings activity.
     */
    public SettingsActivityPublicTransitEntryPoints(
            SettingsActivityTestRule<MainSettings> settingsActivityTestRule) {
        mSettingsActivityTestRule = settingsActivityTestRule;
    }

    /**
     * Starts the test on the main settings page.
     *
     * @return the active entry {@link SettingsStation}
     */
    public SettingsStation startMainSettingsNonBatched() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        SettingsStation entryPageStation = new SettingsStation();
        return sentinel.travelToSync(
                entryPageStation, mSettingsActivityTestRule::startSettingsActivity);
    }

    /**
     * Starts the batched test on the main settings page.
     *
     * @return the active entry {@link SettingsStation}
     */
    public SettingsStation startMainSettings(
            BatchedPublicTransitRule<SettingsStation> batchedRule) {
        return startBatched(batchedRule, this::startMainSettingsNonBatched);
    }

    private SettingsStation startBatched(
            BatchedPublicTransitRule<SettingsStation> batchedRule,
            Callable<SettingsStation> entryPointCallable) {
        mSettingsActivityTestRule.setFinishActivity(false);
        SettingsStation station = batchedRule.getHomeStation();
        if (station == null) {
            try {
                station = entryPointCallable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            sActivity = mSettingsActivityTestRule.getActivity();
        } else {
            mSettingsActivityTestRule.setActivity(sActivity);
        }
        return station;
    }
}
