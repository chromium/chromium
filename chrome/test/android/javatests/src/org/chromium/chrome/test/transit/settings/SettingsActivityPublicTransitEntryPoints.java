// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.settings.SettingsTestRule;

import java.util.concurrent.Callable;

/** Entry points for Public Transit tests that use SettingsActivity. */
public class SettingsActivityPublicTransitEntryPoints {
    private final SettingsTestRule<MainSettings> mSettingsTestRule;
    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule;
    private static ChromeBaseAppCompatActivity sActivity;

    /**
     * Constructs the settings activity entry points for Public Transit tests using
     * {@link SettingsTestRule}.
     *
     * @param settingsTestRule The test rule capable of starting the settings activity.
     */
    public SettingsActivityPublicTransitEntryPoints(
            SettingsTestRule<MainSettings> settingsTestRule) {
        mSettingsTestRule = settingsTestRule;
        mSettingsActivityTestRule = null;
    }

    /**
     * Constructs the settings activity entry points for Public Transit tests using legacy
     * {@link SettingsActivityTestRule}.
     *
     * @deprecated Use {@link #SettingsActivityPublicTransitEntryPoints(SettingsTestRule)} instead.
     */
    @Deprecated
    public SettingsActivityPublicTransitEntryPoints(
            SettingsActivityTestRule<MainSettings> settingsActivityTestRule) {
        mSettingsTestRule = null;
        mSettingsActivityTestRule = settingsActivityTestRule;
    }

    /**
     * Starts the test on the main settings page.
     *
     * @return the active entry {@link SettingsStation}
     */
    public SettingsStation<MainSettings> startMainSettingsNonBatched() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        SettingsStation<MainSettings> entryPageStation = new SettingsStation<>(MainSettings.class);
        return sentinel.runTo(
                        () -> {
                            if (mSettingsTestRule != null) {
                                mSettingsTestRule.startSettingsActivity();
                            } else {
                                mSettingsActivityTestRule.startSettingsActivity();
                            }
                        })
                .arriveAt(entryPageStation);
    }

    /**
     * Starts the batched test on the main settings page.
     *
     * @return the active entry {@link SettingsStation}
     */
    public SettingsStation<MainSettings> startMainSettings(
            BatchedPublicTransitRule<SettingsStation<MainSettings>> batchedRule) {
        return startBatched(batchedRule, this::startMainSettingsNonBatched);
    }

    private SettingsStation<MainSettings> startBatched(
            BatchedPublicTransitRule<SettingsStation<MainSettings>> batchedRule,
            Callable<SettingsStation<MainSettings>> entryPointCallable) {
        if (mSettingsTestRule != null) {
            mSettingsTestRule.setFinishActivity(false);
        } else {
            mSettingsActivityTestRule.setFinishActivity(false);
        }
        SettingsStation<MainSettings> station = batchedRule.getHomeStation();
        if (station == null) {
            try {
                station = entryPointCallable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            if (mSettingsTestRule != null) {
                sActivity = mSettingsTestRule.getActivity();
            } else {
                sActivity = mSettingsActivityTestRule.getActivity();
            }
        } else {
            if (mSettingsTestRule != null) {
                mSettingsTestRule.setActivity(sActivity);
            } else {
                mSettingsActivityTestRule.setActivity((SettingsActivity) sActivity);
            }
        }
        return station;
    }
}
