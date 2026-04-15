// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceProtoMigrationController.MIGRATION_ATTEMPTS_HISTOGRAM;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.InstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.MultiInstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.WindowModeData;
import org.chromium.chrome.browser.multiwindow.MultiWindowMetricsUtils.WindowingMode;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link MultiInstanceProtoMigrationController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiInstanceProtoMigrationControllerUnitTest {
    private static final long MULTI_WINDOW_START_TIME = 100L;
    private static final int MAX_INSTANCE_LIMIT = 5;
    private static final long MAX_COUNT_TIME = 200L;
    private static final int MAX_ACTIVE_INSTANCE_COUNT = 3;
    private static final int MAX_INSTANCE_COUNT = 4;
    private static final int MAX_INSTANCE_COUNT_INCOGNITO = 2;
    private static final long MULTI_INSTANCE_START_TIME = 300L;
    private static final long MULTI_WINDOW_MODE_CYCLE_START_TIME = 400L;

    private static final int INSTANCE_ID_0 = 0;
    private static final long LAST_ACCESSED_TIME = 1000L;
    private static final long CLOSURE_TIME = 1234L;
    private static final int TASK_ID = 10;
    private static final int NORMAL_TAB_COUNT = 20;
    private static final int INCOGNITO_TAB_COUNT = 5;
    private static final int TAB_COUNT_FOR_RELAUNCH = 15;
    private static final int PROFILE_TYPE = 1;
    private static final int LATEST_PERSISTENT_STATE_ID = 100;

    private static final String URL = "https://example.com";
    private static final String TITLE = "Title";
    private static final String CUSTOM_TITLE = "CustomTitle";

    private static final int MODE = WindowingMode.DESKTOP_WINDOW;
    private static final long MODE_START_TIME = 5000L;
    private static final long MODE_DURATION_MS = 60000L;
    private static final String ACTIVITY = "activity1";

    private SharedPreferencesManager mPrefs;

    @Before
    public void setUp() {
        MultiInstancePersistentStore.resetForTesting();
        mPrefs = MultiInstanceSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        mPrefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        for (String key : MultiInstancePreferenceKeys.getAllGlobalKeys()) {
            mPrefs.removeKey(key);
        }
    }

    @Test
    public void testMigrateGlobalKeys() {
        // 1. Write data to shared preferences.
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME, MULTI_WINDOW_START_TIME);
        mPrefs.writeBoolean(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, true);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, MAX_INSTANCE_LIMIT);
        mPrefs.writeBoolean(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                true);
        mPrefs.writeLong(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, MAX_COUNT_TIME);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT,
                MAX_ACTIVE_INSTANCE_COUNT);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, MAX_INSTANCE_COUNT);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO,
                MAX_INSTANCE_COUNT_INCOGNITO);
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_START_TIME, MULTI_INSTANCE_START_TIME);
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME,
                MULTI_WINDOW_MODE_CYCLE_START_TIME);

        // 2. Ensure protobuf is empty.
        MultiInstanceData data = MultiInstancePersistentStore.sData;
        assertNull(data);

        // 3. Start migration.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(MIGRATION_ATTEMPTS_HISTOGRAM, 1)
                        .build();
        assertTrue(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 4. Verify all data has successfully migrated to protobuf.
        data = MultiInstancePersistentStore.sData;
        assertNotNull(data);
        assertEquals(MULTI_WINDOW_START_TIME, data.getMultiWindowStartTime());
        assertTrue(data.getMultiInstanceCloseWindowSkipConfirm());
        assertEquals(MAX_INSTANCE_LIMIT, data.getMultiInstanceMaxInstanceLimit());
        assertTrue(data.getMultiInstanceInstanceLimitDowngradeTriggered());
        assertEquals(MAX_COUNT_TIME, data.getMultiInstanceMaxCountTime());
        assertEquals(MAX_ACTIVE_INSTANCE_COUNT, data.getMultiInstanceMaxActiveInstanceCount());
        assertEquals(MAX_INSTANCE_COUNT, data.getMultiInstanceMaxInstanceCount());
        assertEquals(
                MAX_INSTANCE_COUNT_INCOGNITO, data.getMultiInstanceMaxInstanceCountIncognito());
        assertEquals(MULTI_INSTANCE_START_TIME, data.getMultiInstanceStartTime());
        assertEquals(MULTI_WINDOW_MODE_CYCLE_START_TIME, data.getMultiWindowModeCycleStartTime());

        // 5. Verify shared preferences are cleaned up.
        for (String key : MultiInstancePreferenceKeys.getAllGlobalKeys()) {
            if (key.equals(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE)) {
                assertTrue(mPrefs.readBoolean(key, false));
            } else {
                assertFalse("Key should have been cleaned up: " + key, mPrefs.contains(key));
            }
        }

        // 6. Verify migration attempt count histogram recorded.
        watcher.assertExpected();
    }

    @Test
    public void testMigrateInstanceKeys() {
        // 1. Write data to shared preferences.
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                        INSTANCE_ID_0),
                LAST_ACCESSED_TIME);
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME.createKey(INSTANCE_ID_0),
                CLOSURE_TIME);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(INSTANCE_ID_0),
                TASK_ID);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(INSTANCE_ID_0),
                NORMAL_TAB_COUNT);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                        INSTANCE_ID_0),
                INCOGNITO_TAB_COUNT);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.createKey(
                        INSTANCE_ID_0),
                TAB_COUNT_FOR_RELAUNCH);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(INSTANCE_ID_0),
                PROFILE_TYPE);
        mPrefs.writeInt(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID.createKey(
                        INSTANCE_ID_0),
                LATEST_PERSISTENT_STATE_ID);
        mPrefs.writeString(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_URL.createKey(INSTANCE_ID_0), URL);
        mPrefs.writeString(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(INSTANCE_ID_0), TITLE);
        mPrefs.writeString(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(INSTANCE_ID_0),
                CUSTOM_TITLE);
        mPrefs.writeBoolean(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED.createKey(
                        INSTANCE_ID_0),
                true);
        mPrefs.writeBoolean(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_MARKED_FOR_DELETION.createKey(
                        INSTANCE_ID_0),
                false);

        // 2. Ensure protobuf is empty.
        MultiInstanceData data = MultiInstancePersistentStore.sData;
        assertNull(data);

        // 3. Start migration.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(MIGRATION_ATTEMPTS_HISTOGRAM, 1)
                        .build();
        assertTrue(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 4. Verify all data has successfully migrated to protobuf.
        data = MultiInstancePersistentStore.sData;
        assertNotNull(data);
        assertTrue(data.getInstancesMap().containsKey(INSTANCE_ID_0));
        InstanceData instanceData = data.getInstancesMap().get(INSTANCE_ID_0);
        assertEquals(LAST_ACCESSED_TIME, instanceData.getLastAccessedTime());
        assertEquals(CLOSURE_TIME, instanceData.getClosureTime());
        assertEquals(TASK_ID, instanceData.getTaskId());
        assertEquals(NORMAL_TAB_COUNT, instanceData.getNormalTabCount());
        assertEquals(INCOGNITO_TAB_COUNT, instanceData.getIncognitoTabCount());
        assertEquals(TAB_COUNT_FOR_RELAUNCH, instanceData.getTabCountForRelaunch());
        assertEquals(PROFILE_TYPE, instanceData.getProfileType());
        assertEquals(LATEST_PERSISTENT_STATE_ID, instanceData.getLatestPersistentStateId());
        assertEquals(URL, instanceData.getActiveTabUrl());
        assertEquals(TITLE, instanceData.getActiveTabTitle());
        assertEquals(CUSTOM_TITLE, instanceData.getCustomTitle());
        assertTrue(instanceData.getIncognitoSelected());
        assertFalse(instanceData.getMarkedForDeletion());

        // 5. Verify shared preferences are cleaned up.
        for (KeyPrefix prefix : MultiInstancePreferenceKeys.getPerInstancePrefixes()) {
            String key = prefix.createKey(INSTANCE_ID_0);
            assertFalse("Key should have been cleaned up: " + key, mPrefs.contains(key));
        }

        // 6. Verify migration attempt count histogram recorded.
        watcher.assertExpected();
    }

    @Test
    public void testMigrateWindowModeKeys() {
        // 1. Write data to shared preferences.
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(MODE),
                MODE_START_TIME);
        mPrefs.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(MODE),
                MODE_DURATION_MS);
        Set<String> activities = new HashSet<>();
        activities.add(ACTIVITY);
        mPrefs.writeStringSet(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(MODE),
                activities);

        // 2. Ensure protobuf is empty.
        MultiInstanceData data = MultiInstancePersistentStore.sData;
        assertNull(data);

        // 3. Start migration.
        assertTrue(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 4. Verify all data has successfully migrated to protobuf.
        data = MultiInstancePersistentStore.sData;
        assertNotNull(data);
        assertTrue(data.getWindowModesMap().containsKey(MODE));
        WindowModeData modeData = data.getWindowModesMap().get(MODE);
        assertEquals(MODE_START_TIME, modeData.getStartTime());
        assertEquals(MODE_DURATION_MS, modeData.getDurationMs());
        assertEquals(1, modeData.getActivitiesCount());
        assertEquals(ACTIVITY, modeData.getActivities(0));

        // 5. Verify shared preferences are cleaned up.
        for (KeyPrefix prefix : MultiInstancePreferenceKeys.getWindowModePrefixes()) {
            String key = prefix.createKey(MODE);
            assertFalse("Key should have been cleaned up: " + key, mPrefs.contains(key));
        }
    }

    @Test
    public void testDowngrade() {
        // 1. Prepare protobuf data with all fields.
        InstanceData instanceData =
                InstanceData.newBuilder()
                        .setLastAccessedTime(LAST_ACCESSED_TIME)
                        .setClosureTime(CLOSURE_TIME)
                        .setTaskId(TASK_ID)
                        .setNormalTabCount(NORMAL_TAB_COUNT)
                        .setIncognitoTabCount(INCOGNITO_TAB_COUNT)
                        .setTabCountForRelaunch(TAB_COUNT_FOR_RELAUNCH)
                        .setActiveTabUrl(URL)
                        .setActiveTabTitle(TITLE)
                        .setCustomTitle(CUSTOM_TITLE)
                        .setProfileType(PROFILE_TYPE)
                        .setLatestPersistentStateId(LATEST_PERSISTENT_STATE_ID)
                        .setIncognitoSelected(true)
                        .setMarkedForDeletion(false)
                        .build();

        WindowModeData windowModeData =
                WindowModeData.newBuilder()
                        .setStartTime(MODE_START_TIME)
                        .setDurationMs(MODE_DURATION_MS)
                        .addActivities(ACTIVITY)
                        .build();

        MultiInstanceData data =
                MultiInstanceData.newBuilder()
                        .setMultiWindowStartTime(MULTI_WINDOW_START_TIME)
                        .putInstances(INSTANCE_ID_0, instanceData)
                        .putWindowModes(MODE, windowModeData)
                        .build();
        MultiInstancePersistentStore.initializeFromMigration(data);
        mPrefs.writeInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 1);

        // 2. Start downgrade.
        MultiInstanceProtoMigrationController.getInstance().downgrade();

        // 3. Verify all data has successfully downgraded to SharedPreferences.
        // Global
        assertEquals(
                MULTI_WINDOW_START_TIME,
                mPrefs.readLong(MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME, 0));

        // Instances
        assertEquals(
                LAST_ACCESSED_TIME,
                mPrefs.readLong(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                CLOSURE_TIME,
                mPrefs.readLong(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                TASK_ID,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                NORMAL_TAB_COUNT,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                INCOGNITO_TAB_COUNT,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                TAB_COUNT_FOR_RELAUNCH,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                PROFILE_TYPE,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(
                                INSTANCE_ID_0),
                        0));
        assertEquals(
                LATEST_PERSISTENT_STATE_ID,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID
                                .createKey(INSTANCE_ID_0),
                        0));
        assertEquals(
                URL,
                mPrefs.readString(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_URL.createKey(INSTANCE_ID_0),
                        ""));
        assertEquals(
                TITLE,
                mPrefs.readString(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(INSTANCE_ID_0),
                        ""));
        assertEquals(
                CUSTOM_TITLE,
                mPrefs.readString(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(
                                INSTANCE_ID_0),
                        ""));
        assertTrue(
                mPrefs.readBoolean(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED.createKey(
                                INSTANCE_ID_0),
                        false));
        assertFalse(
                mPrefs.readBoolean(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MARKED_FOR_DELETION.createKey(
                                INSTANCE_ID_0),
                        true));

        // Window Modes
        assertEquals(
                MODE_START_TIME,
                mPrefs.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(MODE),
                        0));
        assertEquals(
                MODE_DURATION_MS,
                mPrefs.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(MODE),
                        0));
        Set<String> activities =
                mPrefs.readStringSet(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(MODE));
        assertNotNull(activities);
        assertTrue(activities.contains(ACTIVITY));

        // 4. Verify cleanup.
        assertFalse(
                mPrefs.readBoolean(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE,
                        false));
        assertNull(MultiInstancePersistentStore.sData);
        assertFalse(
                mPrefs.contains(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS));
    }

    @Test
    public void testParseInstanceId() {
        assertEquals(
                INSTANCE_ID_0,
                MultiInstanceProtoMigrationController.parseInstanceId(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(
                                INSTANCE_ID_0)));
        assertEquals(
                INSTANCE_ID_0,
                MultiInstanceProtoMigrationController.parseInstanceId(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_URL.createKey(INSTANCE_ID_0)));
        assertEquals(
                MultiInstanceManager.INVALID_WINDOW_ID,
                MultiInstanceProtoMigrationController.parseInstanceId(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME));
    }

    @Test
    public void testMigrateRetryLimit() {
        // 1. Set attempt count to limit (3).
        mPrefs.writeInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 3);

        // 2. Migration should return false immediately.
        assertFalse(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 3. Set attempt count to 2.
        mPrefs.writeInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 2);

        // 4. Migration should be allowed.
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(MIGRATION_ATTEMPTS_HISTOGRAM, 3)
                        .build();
        assertTrue(MultiInstanceProtoMigrationController.getInstance().migrate());
        watcher.assertExpected();
    }

    @Test
    public void testMigrateRetryIncrementOnFailure() {
        // 1. Force a failure by putting a wrong type in SharedPreferences.
        // MULTI_INSTANCE_MAX_INSTANCE_LIMIT is expected to be Integer, but we put Long.
        mPrefs.writeLong(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, 5L);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(MIGRATION_ATTEMPTS_HISTOGRAM, 3)
                        .build();

        // 2. Attempt migration. It should fail and return false.
        assertFalse(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 3. Verify attempt count is incremented to 1.
        assertEquals(
                1,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 0));

        // 4. Attempt retry migration. It should fail again.
        assertFalse(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 5. Verify attempt count is incremented to 2.
        assertEquals(
                2,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 0));

        // 6. Attempt retry migration again. It should fail again.
        assertFalse(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 7. Verify attempt count is incremented to 3.
        assertEquals(
                3,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 0));

        // 8. Attempt migration again. It should return false immediately due to limit.
        assertFalse(MultiInstanceProtoMigrationController.getInstance().migrate());

        // 9. Verify attempt count remains 2 (not incremented further).
        assertEquals(
                3,
                mPrefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 0));

        // 10. Verify migration attempt count histogram recorded.
        watcher.assertExpected();
    }
}
