// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.util.AtomicFile;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.MultiInstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.WindowModeData;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/**
 * Manages persisted multi-instance state. This includes information required to track metrics and
 * determine UI behavior.
 */
@NullMarked
public class MultiInstancePersistentStore {
    private static final String TAG = "MultiInstanceStore";
    private static final String FILE_NAME = "multi_instance_data.pb";
    private static final String DIR = "multi_instance";

    protected static @Nullable MultiInstanceData sData;
    private static @Nullable AtomicFile sAtomicFile;
    private static @Nullable SequencedTaskRunner sTaskRunner;

    static {
        ensureInitialized();
    }

    private static SequencedTaskRunner getTaskRunner() {
        if (sTaskRunner == null) {
            sTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
        }
        return sTaskRunner;
    }

    protected MultiInstancePersistentStore() {}

    static SharedPreferencesManager getManager() {
        return MultiInstanceSharedPreferences.getInstance();
    }

    @VisibleForTesting
    static void ensureInitialized() {
        boolean isProtoEnabled = ChromeFeatureList.sMultiInstanceSharedPrefsMigration.isEnabled();
        boolean isMigrationCompleted =
                getManager()
                        .readBoolean(
                                MultiInstancePreferenceKeys.MULTI_INSTANCE_PROTO_MIGRATION_COMPLETE,
                                false);

        // 1. When the feature is disabled, trigger a downgrade if a previous migration is detected.
        if (!isProtoEnabled) {
            if (isMigrationCompleted) {
                MultiInstanceProtoMigrationController.getInstance().downgrade();
            }
        }
        // 2. Migration enabled, execute migration if it hasn't been completed, otherwise read data
        // from the file.
        else if (isMigrationCompleted) {
            sData = loadProtoFromFile();
        } else {
            // Attempt migration. If it fails, sData remains null and the store continues to use
            // SharedPreferences. If it succeeds, #initializeFromMigration handles the rest.
            MultiInstanceProtoMigrationController.getInstance().migrate();
        }
    }

    protected static void initializeFromMigration(
            MultiInstanceData data, @Nullable Callback<Boolean> onComplete) {
        sData = data;
        saveProto(onComplete);
    }

    private static AtomicFile getAtomicFile() {
        if (sAtomicFile == null) {
            File dir = ContextUtils.getApplicationContext().getDir(DIR, Context.MODE_PRIVATE);
            File file = new File(dir, FILE_NAME);
            sAtomicFile = new AtomicFile(file);
        }
        return sAtomicFile;
    }

    protected static @Nullable MultiInstanceData loadProtoFromFile() {
        if (sData != null) return sData;
        AtomicFile atomicFile = getAtomicFile();
        if (!atomicFile.getBaseFile().exists()) return MultiInstanceData.getDefaultInstance();

        FileInputStream stream = null;
        try {
            stream = atomicFile.openRead();
            return MultiInstanceData.parseFrom(stream);
        } catch (IOException e) {
            Log.e(TAG, "Failed to load multi-instance proto", e);
            return MultiInstanceData.getDefaultInstance();
        } finally {
            StreamUtil.closeQuietly(stream);
        }
    }

    protected static void saveProto() {
        saveProto(null);
    }

    protected static void saveProto(@Nullable Callback<Boolean> onComplete) {
        MultiInstanceData data = sData;
        if (data == null) {
            if (onComplete != null) onComplete.onResult(false);
            return;
        }

        getTaskRunner()
                .execute(
                        () -> {
                            boolean success = false;
                            AtomicFile atomicFile = getAtomicFile();
                            FileOutputStream stream = null;
                            try {
                                stream = atomicFile.startWrite();
                                data.writeTo(stream);
                                atomicFile.finishWrite(stream);
                                success = true;
                            } catch (IOException e) {
                                if (stream != null) atomicFile.failWrite(stream);
                                Log.e(TAG, "Failed to save multi-instance proto", e);
                            }

                            if (onComplete != null) {
                                final boolean finalSuccess = success;
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> onComplete.onResult(finalSuccess));
                            }
                        });
    }

    protected static void deleteProtoFile() {
        sData = null;
        getTaskRunner().execute(() -> getAtomicFile().delete());
    }

    public static boolean containsMultiWindowModeCycleStartTime() {
        if (sData != null) {
            return sData.hasMultiWindowModeCycleStartTime();
        }
        return getManager()
                .contains(MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME);
    }

    public static boolean containsMultiWindowModeStartTime(int modeIndex) {
        if (sData != null) {
            WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
            return windowModeData != null && windowModeData.hasStartTime();
        } else {
            String key =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                            String.valueOf(modeIndex));
            return getManager().contains(key);
        }
    }

    static long readMultiWindowStartTime() {
        if (sData != null) return sData.getMultiWindowStartTime();
        return getManager().readLong(MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME, 0);
    }

    static void writeMultiWindowStartTime(long startTime) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiWindowStartTime(startTime).build();
            saveProto();
        } else {
            getManager().writeLong(MultiInstancePreferenceKeys.MULTI_WINDOW_START_TIME, startTime);
        }
    }

    static boolean readCloseWindowSkipConfirm() {
        if (sData != null) return sData.getMultiInstanceCloseWindowSkipConfirm();
        return getManager()
                .readBoolean(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM,
                        false);
    }

    static void writeCloseWindowSkipConfirm(boolean skipConfirm) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceCloseWindowSkipConfirm(skipConfirm).build();
            saveProto();
        } else {
            getManager()
                    .writeBoolean(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM,
                            skipConfirm);
        }
    }

    static int readMaxInstanceLimit(int maxInstance) {
        if (sData != null) {
            return sData.hasMultiInstanceMaxInstanceLimit()
                    ? sData.getMultiInstanceMaxInstanceLimit()
                    : maxInstance;
        }
        return getManager()
                .readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, maxInstance);
    }

    static void writeMaxInstanceLimit(int maxInstance) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceMaxInstanceLimit(maxInstance).build();
            saveProto();
        } else {
            getManager()
                    .writeInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT,
                            maxInstance);
        }
    }

    static boolean readInstanceLimitDowngradeTriggered() {
        if (sData != null) {
            return sData.getMultiInstanceInstanceLimitDowngradeTriggered();
        }
        return getManager()
                .readBoolean(
                        MultiInstancePreferenceKeys
                                .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                        false);
    }

    static void writeInstanceLimitDowngradeTriggered(boolean triggered) {
        if (sData != null) {
            sData =
                    sData.toBuilder()
                            .setMultiInstanceInstanceLimitDowngradeTriggered(triggered)
                            .build();
            saveProto();
        } else {
            getManager()
                    .writeBoolean(
                            MultiInstancePreferenceKeys
                                    .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                            triggered);
        }
    }

    static long readMaxCountHistogramStartTime() {
        if (sData != null) {
            return sData.getMultiInstanceMaxCountTime();
        }
        return getManager().readLong(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, 0);
    }

    static void writeMaxCountHistogramStartTime(long maxCountTime) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceMaxCountTime(maxCountTime).build();
            saveProto();
        } else {
            getManager()
                    .writeLong(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME,
                            maxCountTime);
        }
    }

    static int readDailyMaxActiveInstanceCount() {
        if (sData != null) {
            return sData.getMultiInstanceMaxActiveInstanceCount();
        }
        return getManager()
                .readInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT, 0);
    }

    static void writeDailyMaxActiveInstanceCount(int count) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceMaxActiveInstanceCount(count).build();
            saveProto();
        } else {
            getManager()
                    .writeInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT,
                            count);
        }
    }

    static int readDailyMaxInstanceCount() {
        if (sData != null) {
            return sData.getMultiInstanceMaxInstanceCount();
        }
        return getManager()
                .readInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, 0);
    }

    static void writeDailyMaxInstanceCount(int count) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceMaxInstanceCount(count).build();
            saveProto();
        } else {
            getManager()
                    .writeInt(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, count);
        }
    }

    static int readDailyMaxIncognitoInstanceCount() {
        if (sData != null) return sData.getMultiInstanceMaxInstanceCountIncognito();
        return getManager()
                .readInt(
                        MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO, 0);
    }

    static void writeDailyMaxIncognitoInstanceCount(int count) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceMaxInstanceCountIncognito(count).build();
            saveProto();
        } else {
            getManager()
                    .writeInt(
                            MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO,
                            count);
        }
    }

    static long readMultiInstanceStartTime() {
        if (sData != null) return sData.getMultiInstanceStartTime();
        return getManager().readLong(MultiInstancePreferenceKeys.MULTI_INSTANCE_START_TIME, 0);
    }

    static void writeMultiInstanceStartTime(long startTime) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiInstanceStartTime(startTime).build();
            saveProto();
        } else {
            getManager()
                    .writeLong(MultiInstancePreferenceKeys.MULTI_INSTANCE_START_TIME, startTime);
        }
    }

    static long readMultiWindowModeCycleStartTime() {
        if (sData != null) {
            return sData.getMultiWindowModeCycleStartTime();
        }
        return getManager()
                .readLong(MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, 0);
    }

    static void writeMultiWindowModeCycleStartTime(long startTime) {
        if (sData != null) {
            sData = sData.toBuilder().setMultiWindowModeCycleStartTime(startTime).build();
            saveProto();
        } else {
            getManager()
                    .writeLong(
                            MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME,
                            startTime);
        }
    }

    static long readMultiWindowModeStartTime(int modeIndex, long currentTime) {
        if (sData != null) {
            WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
            return (wm != null && wm.hasStartTime()) ? wm.getStartTime() : currentTime;
        }
        String startTimeKey =
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
        return getManager().readLong(startTimeKey, currentTime);
    }

    static void writeMultiWindowModeStartTime(int modeIndex, long startTime) {
        if (sData != null) {
            WindowModeData wm =
                    sData
                            .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                            .toBuilder()
                            .setStartTime(startTime)
                            .build();
            sData = sData.toBuilder().putWindowModes(modeIndex, wm).build();
            saveProto();
        } else {
            String startTimeKey =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
            getManager().writeLong(startTimeKey, startTime);
        }
    }

    static long readMultiWindowModeDurationMs(int modeIndex) {
        if (sData != null) {
            WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
            return windowModeData != null ? windowModeData.getDurationMs() : 0;
        }
        String durationKey =
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
        return getManager().readLong(durationKey, 0);
    }

    static void writeMultiWindowModeDurationMs(int modeIndex, long duration) {
        if (sData != null) {
            WindowModeData windowModeData =
                    sData
                            .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                            .toBuilder()
                            .setDurationMs(duration)
                            .build();
            sData = sData.toBuilder().putWindowModes(modeIndex, windowModeData).build();
            saveProto();
        } else {
            String durationKey =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
            getManager().writeLong(durationKey, duration);
        }
    }

    static @Nullable Set<String> readMultiWindowModeActivities(int modeIndex) {
        if (sData != null) {
            WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
            return (windowModeData != null && windowModeData.getActivitiesCount() > 0)
                    ? new HashSet<>(windowModeData.getActivitiesList())
                    : null;
        }
        String activitiesKey =
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(modeIndex);
        return getManager().readStringSet(activitiesKey, null);
    }

    static void writeMultiWindowModeActivities(int modeIndex, Set<String> activities) {
        if (sData != null) {
            WindowModeData windowModeData =
                    sData
                            .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                            .toBuilder()
                            .clearActivities()
                            .addAllActivities(activities)
                            .build();
            sData = sData.toBuilder().putWindowModes(modeIndex, windowModeData).build();
            saveProto();
        } else {
            String activitiesKey =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(modeIndex);
            getManager().writeStringSet(activitiesKey, activities);
        }
    }

    static void removeMultiWindowModeStartTime(int modeIndex) {
        if (sData != null) {
            WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
            if (wm != null) {
                sData =
                        sData.toBuilder()
                                .putWindowModes(modeIndex, wm.toBuilder().clearStartTime().build())
                                .build();
                saveProto();
            }
        } else {
            String startTimeKey =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
            getManager().removeKey(startTimeKey);
        }
    }

    static void removeMultiWindowModeDurationMs(int modeIndex) {
        if (sData != null) {
            WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
            if (wm != null) {
                sData =
                        sData.toBuilder()
                                .putWindowModes(modeIndex, wm.toBuilder().clearDurationMs().build())
                                .build();
                saveProto();
            }
        } else {
            String durationKey =
                    MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
            getManager().removeKey(durationKey);
        }
    }

    static boolean contains(String key) {
        return getManager().contains(key);
    }

    public static void resetForTesting() {
        sData = null;

        // Delete the physical file so the next 'enabled' run starts fresh.
        if (sAtomicFile != null) {
            sAtomicFile.delete();
            sAtomicFile = null;
        }
    }
}
