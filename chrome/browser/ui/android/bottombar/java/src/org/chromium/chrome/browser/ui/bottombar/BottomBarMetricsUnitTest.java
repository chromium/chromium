// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link BottomBarMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp")
public class BottomBarMetricsUnitTest {
    private static final String ENABLED_STATE_HISTOGRAM = "Android.BottomBar.EnabledState";

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeRecordEnabledState_enabled_recordsTrue() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(ENABLED_STATE_HISTOGRAM, true);

        BottomBarMetrics.maybeRecordEnabledState(mContext);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeRecordEnabledState_userDisabled_recordsFalse() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(ENABLED_STATE_HISTOGRAM, false);

        BottomBarMetrics.maybeRecordEnabledState(mContext);

        watcher.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeRecordEnabledState_featureDisabled_doesNotRecord() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(ENABLED_STATE_HISTOGRAM).build();

        BottomBarMetrics.maybeRecordEnabledState(mContext);

        watcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeRecordEnabledState_tablet_doesNotRecord() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(ENABLED_STATE_HISTOGRAM).build();

        BottomBarMetrics.maybeRecordEnabledState(mContext);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeRecordEnabledState_automotive_doesNotRecord() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(ENABLED_STATE_HISTOGRAM).build();

        BottomBarMetrics.maybeRecordEnabledState(mContext);

        watcher.assertExpected();
    }
}
