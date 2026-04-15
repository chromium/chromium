// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.InstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.MultiInstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.WindowModeData;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Controller for migrating multi-instance related data between SharedPreferences and protobuf. */
@NullMarked
class MultiInstanceProtoMigrationController {
    private static final String TAG = "MultiInstMigration";
    private static final int INVALID_MODE = -1;

    /** Total number of migration attempts allowed. Includes the first attempt plus two retries. */
    private static final int MIGRATION_ATTEMPT_LIMIT = 3;

    static final String MIGRATION_ATTEMPTS_HISTOGRAM = "Android.MultiInstance.MigrationAttempts";

    private static final MultiInstanceProtoMigrationController sInstance =
            new MultiInstanceProtoMigrationController();

    static MultiInstanceProtoMigrationController getInstance() {
        return sInstance;
    }

    private MultiInstanceProtoMigrationController() {}

    boolean migrate() {
        SharedPreferencesManager prefs = MultiInstanceSharedPreferences.getInstance();

        int attemptCount =
                prefs.readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS, 0);
        if (attemptCount == MIGRATION_ATTEMPT_LIMIT) {
            RecordHistogram.recordExactLinearHistogram(
                    MIGRATION_ATTEMPTS_HISTOGRAM,
                    MIGRATION_ATTEMPT_LIMIT,
                    MIGRATION_ATTEMPT_LIMIT + 1);
            return false;
        }

        try {
            MultiInstanceData.Builder builder = MultiInstanceData.newBuilder();
            Map<String, ?> allPrefs = ContextUtils.getAppSharedPreferences().getAll();
            Map<Integer, InstanceData.Builder> instanceBuilders = new HashMap<>();
            Map<Integer, WindowModeData.Builder> windowModeBuilders = new HashMap<>();

            for (Map.Entry<String, ?> entry : allPrefs.entrySet()) {
                String key = entry.getKey();
                Object val = entry.getValue();
                if (val == null) {
                    continue;
                }

                // 1. Migrate global data to protobuf and cast each value to its original type.
                if (MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM.equals(
                        key)) {
                    builder.setMultiInstanceCloseWindowSkipConfirm((Boolean) val);
                } else if (MultiInstancePreferenceKeys
                        .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED
                        .equals(key)) {
                    builder.setMultiInstanceInstanceLimitDowngradeTriggered((Boolean) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT.equals(
                        key)) {
                    builder.setMultiInstanceMaxInstanceLimit((Integer) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT
                        .equals(key)) {
                    builder.setMultiInstanceMaxActiveInstanceCount((Integer) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT.equals(
                        key)) {
                    builder.setMultiInstanceMaxInstanceCount((Integer) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO
                        .equals(key)) {
                    builder.setMultiInstanceMaxInstanceCountIncognito((Integer) val);
                } else if (MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME.equals(key)) {
                    builder.setMultiWindowStartTime((Long) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME.equals(key)) {
                    builder.setMultiInstanceMaxCountTime((Long) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_START_TIME.equals(key)) {
                    builder.setMultiInstanceStartTime((Long) val);
                } else if (MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME.equals(
                        key)) {
                    builder.setMultiWindowModeCycleStartTime((Long) val);
                } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE
                                .equals(key)
                        || MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS
                                .equals(key)) {
                } else {
                    // 2. Migrate per-instance data to protobuf(prefix matching).
                    int instanceId = parseInstanceId(key);
                    if (instanceId != INVALID_WINDOW_ID) {
                        populateInstanceBuilder(
                                instanceBuilders.computeIfAbsent(
                                        instanceId, k -> InstanceData.newBuilder()),
                                key,
                                val,
                                instanceId);
                    } else {
                        // 3. Migrate activity mode related data to protobuf(prefix matching).
                        int mode = parseModeIndex(key);
                        if (mode != INVALID_MODE) {
                            populateWindowModeBuilder(
                                    windowModeBuilders.computeIfAbsent(
                                            mode, k -> WindowModeData.newBuilder()),
                                    key,
                                    val,
                                    mode);
                        }
                    }
                }
            }

            // 4. Builds instance and window mode data into the Protobuf message.
            for (Map.Entry<Integer, InstanceData.Builder> entry : instanceBuilders.entrySet()) {
                builder.putInstances(entry.getKey(), entry.getValue().build());
            }
            for (Map.Entry<Integer, WindowModeData.Builder> entry : windowModeBuilders.entrySet()) {
                builder.putWindowModes(entry.getKey(), entry.getValue().build());
            }

            // 5. Notify MultiInstancePersistentStore
            MultiInstancePersistentStore.initializeFromMigration(builder.build());
            prefs.writeBoolean(
                    MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE, true);
            cleanupOldKeys(prefs);
            RecordHistogram.recordExactLinearHistogram(
                    MIGRATION_ATTEMPTS_HISTOGRAM, attemptCount + 1, MIGRATION_ATTEMPT_LIMIT + 1);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Migration failed", e);
            prefs.writeInt(
                    MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS,
                    attemptCount + 1);
            return false;
        }
    }

    void downgrade() {
        MultiInstanceData data = MultiInstancePersistentStore.loadProtoFromFile();
        if (data == null) {
            return;
        }
        SharedPreferencesManager prefs = MultiInstanceSharedPreferences.getInstance();
        try {
            SharedPreferences.Editor editor = prefs.getEditor();

            // Downgrade instance global data.
            if (data.hasMultiInstanceCloseWindowSkipConfirm()) {
                editor.putBoolean(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM,
                        data.getMultiInstanceCloseWindowSkipConfirm());
            }
            if (data.hasMultiInstanceInstanceLimitDowngradeTriggered()) {
                editor.putBoolean(
                        MultiInstancePreferenceKeys
                                .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                        data.getMultiInstanceInstanceLimitDowngradeTriggered());
            }
            if (data.hasMultiInstanceMaxInstanceLimit()) {
                editor.putInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT,
                        data.getMultiInstanceMaxInstanceLimit());
            }
            if (data.hasMultiInstanceMaxActiveInstanceCount()) {
                editor.putInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT,
                        data.getMultiInstanceMaxActiveInstanceCount());
            }
            if (data.hasMultiInstanceMaxInstanceCount()) {
                editor.putInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT,
                        data.getMultiInstanceMaxInstanceCount());
            }
            if (data.hasMultiInstanceMaxInstanceCountIncognito()) {
                editor.putInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO,
                        data.getMultiInstanceMaxInstanceCountIncognito());
            }
            if (data.hasMultiWindowStartTime()) {
                editor.putLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME,
                        data.getMultiWindowStartTime());
            }
            if (data.hasMultiInstanceMaxCountTime()) {
                editor.putLong(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME,
                        data.getMultiInstanceMaxCountTime());
            }
            if (data.hasMultiInstanceStartTime()) {
                editor.putLong(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_START_TIME,
                        data.getMultiInstanceStartTime());
            }
            if (data.hasMultiWindowModeCycleStartTime()) {
                editor.putLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME,
                        data.getMultiWindowModeCycleStartTime());
            }

            // Downgrade instance specific data.
            for (Map.Entry<Integer, InstanceData> entry : data.getInstancesMap().entrySet()) {
                int instanceId = entry.getKey();
                InstanceData instanceData = entry.getValue();
                if (instanceData.hasIncognitoSelected()) {
                    editor.putBoolean(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED
                                    .createKey(instanceId),
                            instanceData.getIncognitoSelected());
                }
                if (instanceData.hasMarkedForDeletion()) {
                    editor.putBoolean(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_MARKED_FOR_DELETION
                                    .createKey(instanceId),
                            instanceData.getMarkedForDeletion());
                }
                if (instanceData.hasTaskId()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(
                                    instanceId),
                            instanceData.getTaskId());
                }
                if (instanceData.hasNormalTabCount()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(
                                    instanceId),
                            instanceData.getNormalTabCount());
                }
                if (instanceData.hasIncognitoTabCount()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT
                                    .createKey(instanceId),
                            instanceData.getIncognitoTabCount());
                }
                if (instanceData.hasTabCountForRelaunch()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH
                                    .createKey(instanceId),
                            instanceData.getTabCountForRelaunch());
                }
                if (instanceData.hasProfileType()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(
                                    instanceId),
                            instanceData.getProfileType());
                }
                if (instanceData.hasLatestPersistentStateId()) {
                    editor.putInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID
                                    .createKey(instanceId),
                            instanceData.getLatestPersistentStateId());
                }
                if (instanceData.hasLastAccessedTime()) {
                    editor.putLong(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                                    instanceId),
                            instanceData.getLastAccessedTime());
                }
                if (instanceData.hasClosureTime()) {
                    editor.putLong(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME.createKey(
                                    instanceId),
                            instanceData.getClosureTime());
                }
                if (instanceData.hasActiveTabUrl()) {
                    editor.putString(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_URL.createKey(instanceId),
                            instanceData.getActiveTabUrl());
                }
                if (instanceData.hasActiveTabTitle()) {
                    editor.putString(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(instanceId),
                            instanceData.getActiveTabTitle());
                }
                if (instanceData.hasCustomTitle()) {
                    editor.putString(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(
                                    instanceId),
                            instanceData.getCustomTitle());
                }
            }

            // Downgrade window modes data.
            for (Map.Entry<Integer, WindowModeData> entry : data.getWindowModesMap().entrySet()) {
                int id = entry.getKey();
                WindowModeData windowModeData = entry.getValue();
                if (windowModeData.hasStartTime()) {
                    editor.putLong(
                            MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(id),
                            windowModeData.getStartTime());
                }
                if (windowModeData.hasDurationMs()) {
                    editor.putLong(
                            MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(id),
                            windowModeData.getDurationMs());
                }
                if (windowModeData.getActivitiesCount() > 0) {
                    editor.putStringSet(
                            MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(id),
                            new HashSet<>(windowModeData.getActivitiesList()));
                }
            }

            editor.remove(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE);
            editor.remove(MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_ATTEMPTS);
            editor.commit();
            MultiInstancePersistentStore.deleteProtoFile();
        } catch (Exception e) {
            Log.e(TAG, "Downgrade failed", e);
        }
    }

    @VisibleForTesting
    static int parseInstanceId(String key) {
        for (KeyPrefix keyPrefix : MultiInstancePreferenceKeys.getPerInstancePrefixes()) {
            if (keyPrefix.hasGenerated(key)) {
                try {
                    return Integer.parseInt(key.substring(keyPrefix.pattern().length() - 1));
                } catch (Exception e) {
                    return INVALID_WINDOW_ID;
                }
            }
        }
        return INVALID_WINDOW_ID;
    }

    private int parseModeIndex(String key) {
        for (KeyPrefix keyPrefix : MultiInstancePreferenceKeys.getWindowModePrefixes()) {
            if (keyPrefix.hasGenerated(key)) {
                try {
                    return Integer.parseInt(key.substring(keyPrefix.pattern().length() - 1));
                } catch (Exception e) {
                    return INVALID_MODE;
                }
            }
        }
        return INVALID_MODE;
    }

    private void populateInstanceBuilder(
            InstanceData.Builder instanceDataBuilder, String key, Object value, int instanceId) {
        if (MultiInstancePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setIncognitoSelected((Boolean) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_MARKED_FOR_DELETION
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setMarkedForDeletion((Boolean) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setTaskId((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setNormalTabCount((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setIncognitoTabCount((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setTabCountForRelaunch((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setProfileType((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setLatestPersistentStateId((Integer) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setLastAccessedTime((Long) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setClosureTime((Long) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_URL
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setActiveTabUrl((String) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_TITLE
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setActiveTabTitle((String) value);
        } else if (MultiInstancePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE
                .createKey(instanceId)
                .equals(key)) {
            instanceDataBuilder.setCustomTitle((String) value);
        }
    }

    @SuppressWarnings("unchecked") // Set<String> from SharedPreferences can't verify element type.
    private void populateWindowModeBuilder(
            WindowModeData.Builder windowModeBuilder, String key, Object value, int mode) {
        if (MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(mode).equals(key)) {
            windowModeBuilder.setStartTime((Long) value);
        } else if (MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS
                .createKey(mode)
                .equals(key)) {
            windowModeBuilder.setDurationMs((Long) value);
        } else if (MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES
                .createKey(mode)
                .equals(key)) {
            if (value instanceof Set) {
                windowModeBuilder.addAllActivities((Set<String>) value);
            }
        }
    }

    private void cleanupOldKeys(SharedPreferencesManager prefs) {
        // Remove global keys.
        for (String key : MultiInstancePreferenceKeys.getAllGlobalKeys()) {
            prefs.removeKey(key);
        }

        // Remove prefixed keys.
        for (KeyPrefix prefix : MultiInstancePreferenceKeys.getPerInstancePrefixes()) {
            prefs.removeKeysWithPrefix(prefix);
        }
        for (KeyPrefix prefix : MultiInstancePreferenceKeys.getWindowModePrefixes()) {
            prefs.removeKeysWithPrefix(prefix);
        }
    }
}
