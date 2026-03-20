// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowMetricsUtils.WindowingMode;

import java.util.HashSet;
import java.util.Set;

/**
 * Unit tests for {@link MultiInstancePersistentStore}. Focuses on the Protobuf implementation for
 * global fields.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.MULTI_INSTANCE_SHARED_PREFS_MIGRATION)
public class MultiInstancePersistentStoreUnitTest {

    @Before
    public void setUp() {
        MultiInstancePersistentStore.ensureInitialized();
    }

    @After
    public void tearDown() {
        MultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testMultiWindowModeCycleStartTime() {
        // Test default value.
        assertFalse(MultiInstancePersistentStore.containsMultiWindowModeCycleStartTime());
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowModeCycleStartTime());

        // Verify that the value is successfully written to the store.
        long startTime = 11111L;
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(startTime);
        assertTrue(MultiInstancePersistentStore.containsMultiWindowModeCycleStartTime());
        assertEquals(startTime, MultiInstancePersistentStore.readMultiWindowModeCycleStartTime());
    }

    @Test
    public void testMultiWindowModeStartTime() {
        int modeIndex = WindowingMode.DESKTOP_WINDOW;
        long startTime = 22222L;

        // Test default value.
        assertFalse(MultiInstancePersistentStore.containsMultiWindowModeStartTime(modeIndex));
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowModeStartTime(modeIndex, 0L));

        // Verify that the value is successfully written to the store.
        MultiInstancePersistentStore.writeMultiWindowModeStartTime(modeIndex, startTime);
        assertTrue(MultiInstancePersistentStore.containsMultiWindowModeStartTime(modeIndex));
        assertEquals(
                startTime,
                MultiInstancePersistentStore.readMultiWindowModeStartTime(modeIndex, 0L));

        // Verify that the value is successfully cleared.
        MultiInstancePersistentStore.removeMultiWindowModeStartTime(modeIndex);
        assertFalse(MultiInstancePersistentStore.containsMultiWindowModeStartTime(modeIndex));
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowModeStartTime(modeIndex, 0L));
    }

    @Test
    public void testMultiWindowStartTime() {
        // Test default value.
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowStartTime());

        // Verify that the value is successfully written to the store.
        long startTime = 12345L;
        MultiInstancePersistentStore.writeMultiWindowStartTime(startTime);
        assertEquals(startTime, MultiInstancePersistentStore.readMultiWindowStartTime());
    }

    @Test
    public void testCloseWindowSkipConfirm() {
        // Test default value.
        assertFalse(MultiInstancePersistentStore.readCloseWindowSkipConfirm());

        // Verify that the value is successfully written to the store.
        MultiInstancePersistentStore.writeCloseWindowSkipConfirm(true);
        assertTrue(MultiInstancePersistentStore.readCloseWindowSkipConfirm());

        MultiInstancePersistentStore.writeCloseWindowSkipConfirm(false);
        assertFalse(MultiInstancePersistentStore.readCloseWindowSkipConfirm());
    }

    @Test
    public void testMaxInstanceLimit() {
        // Test default value.
        int defaultLimit = 5;
        assertEquals(defaultLimit, MultiInstancePersistentStore.readMaxInstanceLimit(defaultLimit));

        // Verify that the value is successfully written to the store.
        int limit = 10;
        MultiInstancePersistentStore.writeMaxInstanceLimit(limit);
        assertEquals(limit, MultiInstancePersistentStore.readMaxInstanceLimit(defaultLimit));
    }

    @Test
    public void testInstanceLimitDowngradeTriggered() {
        // Test default value.
        assertFalse(MultiInstancePersistentStore.readInstanceLimitDowngradeTriggered());

        // Verify that the value is successfully written to the store.
        MultiInstancePersistentStore.writeInstanceLimitDowngradeTriggered(true);
        assertTrue(MultiInstancePersistentStore.readInstanceLimitDowngradeTriggered());

        MultiInstancePersistentStore.writeInstanceLimitDowngradeTriggered(false);
        assertFalse(MultiInstancePersistentStore.readInstanceLimitDowngradeTriggered());
    }

    @Test
    public void testMaxCountHistogramStartTime() {
        // Test default value.
        assertEquals(0L, MultiInstancePersistentStore.readMaxCountHistogramStartTime());

        // Verify that the value is successfully written to the store.
        long startTime = 54321L;
        MultiInstancePersistentStore.writeMaxCountHistogramStartTime(startTime);
        assertEquals(startTime, MultiInstancePersistentStore.readMaxCountHistogramStartTime());
    }

    @Test
    public void testDailyMaxActiveInstanceCount() {
        // Test default value.
        assertEquals(0, MultiInstancePersistentStore.readDailyMaxActiveInstanceCount());

        // Verify that the value is successfully written to the store.
        int instanceCount = 7;
        MultiInstancePersistentStore.writeDailyMaxActiveInstanceCount(instanceCount);
        assertEquals(instanceCount, MultiInstancePersistentStore.readDailyMaxActiveInstanceCount());
    }

    @Test
    public void testDailyMaxInstanceCount() {
        // Test default value.
        assertEquals(0, MultiInstancePersistentStore.readDailyMaxInstanceCount());

        // Verify that the value is successfully written to the store.
        int instanceCount = 12;
        MultiInstancePersistentStore.writeDailyMaxInstanceCount(instanceCount);
        assertEquals(instanceCount, MultiInstancePersistentStore.readDailyMaxInstanceCount());
    }

    @Test
    public void testDailyMaxIncognitoInstanceCount() {
        // Test default value.
        assertEquals(0, MultiInstancePersistentStore.readDailyMaxIncognitoInstanceCount());

        // Verify that the value is successfully written to the store.
        int instanceCount = 3;
        MultiInstancePersistentStore.writeDailyMaxIncognitoInstanceCount(instanceCount);
        assertEquals(
                instanceCount, MultiInstancePersistentStore.readDailyMaxIncognitoInstanceCount());
    }

    @Test
    public void testMultiInstanceStartTime() {
        // Test default value.
        assertEquals(0L, MultiInstancePersistentStore.readMultiInstanceStartTime());

        // Verify that the value is successfully written to the store.
        long startTime = 99999L;
        MultiInstancePersistentStore.writeMultiInstanceStartTime(startTime);
        assertEquals(startTime, MultiInstancePersistentStore.readMultiInstanceStartTime());
    }

    @Test
    public void testMultiWindowModeDurationMs() {
        int modeIndex = WindowingMode.FULLSCREEN;
        long duration = 33333L;
        // Test default value.
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowModeDurationMs(modeIndex));

        // Verify that the value is successfully written to the store.
        MultiInstancePersistentStore.writeMultiWindowModeDurationMs(modeIndex, duration);
        assertEquals(
                duration, MultiInstancePersistentStore.readMultiWindowModeDurationMs(modeIndex));

        // Verify that the value is successfully cleared.
        MultiInstancePersistentStore.removeMultiWindowModeDurationMs(modeIndex);
        assertEquals(0L, MultiInstancePersistentStore.readMultiWindowModeDurationMs(modeIndex));
    }

    @Test
    public void testMultiWindowModeActivities() {
        int modeIndex = WindowingMode.MULTI_WINDOW;
        Set<String> activities = new HashSet<>();
        activities.add("Activity1");
        activities.add("Activity2");

        // Test default value.
        assertNull(MultiInstancePersistentStore.readMultiWindowModeActivities(modeIndex));

        // Verify that the value is successfully written to the store.
        MultiInstancePersistentStore.writeMultiWindowModeActivities(modeIndex, activities);
        assertEquals(
                activities, MultiInstancePersistentStore.readMultiWindowModeActivities(modeIndex));

        // Verify that the value is successfully cleared.
        MultiInstancePersistentStore.writeMultiWindowModeActivities(modeIndex, new HashSet<>());
        assertNull(MultiInstancePersistentStore.readMultiWindowModeActivities(modeIndex));
    }
}
