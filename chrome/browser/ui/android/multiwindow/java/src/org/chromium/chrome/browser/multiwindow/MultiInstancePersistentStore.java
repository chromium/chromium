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
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.MultiInstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.WindowModeData;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
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

    @VisibleForTesting
    static void ensureInitialized() {
        if (sData != null) return;
        sData = loadProtoFromFile();
    }

    private static AtomicFile getAtomicFile() {
        if (sAtomicFile == null) {
            File dir = ContextUtils.getApplicationContext().getDir(DIR, Context.MODE_PRIVATE);
            File file = new File(dir, FILE_NAME);
            sAtomicFile = new AtomicFile(file);
        }
        return sAtomicFile;
    }

    protected static MultiInstanceData loadProtoFromFile() {
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
                                        TaskTraits.UI_DEFAULT, onComplete.bind(finalSuccess));
                            }
                        });
    }

    protected static void deleteProtoFile() {
        sData = null;
        getTaskRunner().execute(() -> getAtomicFile().delete());
    }

    static boolean containsMultiWindowModeCycleStartTime() {
        assert sData != null;
        return sData.hasMultiWindowModeCycleStartTime();
    }

    static boolean containsMultiWindowModeStartTime(int modeIndex) {
        assert sData != null;
        WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
        return windowModeData != null && windowModeData.hasStartTime();
    }

    @VisibleForTesting
    static boolean containsMultiWindowModeDurationMs(int modeIndex) {
        assert sData != null;
        WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
        return windowModeData != null && windowModeData.hasDurationMs();
    }

    static long readMultiWindowStartTime() {
        assert sData != null;
        return sData.getMultiWindowStartTime();
    }

    static void writeMultiWindowStartTime(long startTime) {
        assert sData != null;
        sData = sData.toBuilder().setMultiWindowStartTime(startTime).build();
        saveProto();
    }

    static boolean readCloseWindowSkipConfirm() {
        assert sData != null;
        return sData.getMultiInstanceCloseWindowSkipConfirm();
    }

    static void writeCloseWindowSkipConfirm(boolean skipConfirm) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceCloseWindowSkipConfirm(skipConfirm).build();
        saveProto();
    }

    static int readMaxInstanceLimit(int maxInstance) {
        assert sData != null;
        return sData.hasMultiInstanceMaxInstanceLimit()
                ? sData.getMultiInstanceMaxInstanceLimit()
                : maxInstance;
    }

    static void writeMaxInstanceLimit(int maxInstance) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceMaxInstanceLimit(maxInstance).build();
        saveProto();
    }

    static boolean readInstanceLimitDowngradeTriggered() {
        assert sData != null;
        return sData.getMultiInstanceInstanceLimitDowngradeTriggered();
    }

    static void writeInstanceLimitDowngradeTriggered(boolean triggered) {
        assert sData != null;
        sData =
                sData.toBuilder()
                        .setMultiInstanceInstanceLimitDowngradeTriggered(triggered)
                        .build();
        saveProto();
    }

    static long readMaxCountHistogramStartTime() {
        assert sData != null;
        return sData.getMultiInstanceMaxCountTime();
    }

    static void writeMaxCountHistogramStartTime(long maxCountTime) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceMaxCountTime(maxCountTime).build();
        saveProto();
    }

    static int readDailyMaxActiveInstanceCount() {
        assert sData != null;
        return sData.getMultiInstanceMaxActiveInstanceCount();
    }

    static void writeDailyMaxActiveInstanceCount(int count) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceMaxActiveInstanceCount(count).build();
        saveProto();
    }

    static int readDailyMaxInstanceCount() {
        assert sData != null;
        return sData.getMultiInstanceMaxInstanceCount();
    }

    static void writeDailyMaxInstanceCount(int count) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceMaxInstanceCount(count).build();
        saveProto();
    }

    static int readDailyMaxIncognitoInstanceCount() {
        assert sData != null;
        return sData.getMultiInstanceMaxInstanceCountIncognito();
    }

    static void writeDailyMaxIncognitoInstanceCount(int count) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceMaxInstanceCountIncognito(count).build();
        saveProto();
    }

    static long readMultiInstanceStartTime() {
        assert sData != null;
        return sData.getMultiInstanceStartTime();
    }

    static void writeMultiInstanceStartTime(long startTime) {
        assert sData != null;
        sData = sData.toBuilder().setMultiInstanceStartTime(startTime).build();
        saveProto();
    }

    static long readMultiWindowModeCycleStartTime() {
        assert sData != null;
        return sData.getMultiWindowModeCycleStartTime();
    }

    static void writeMultiWindowModeCycleStartTime(long startTime) {
        assert sData != null;
        sData = sData.toBuilder().setMultiWindowModeCycleStartTime(startTime).build();
        saveProto();
    }

    static long readMultiWindowModeStartTime(int modeIndex, long currentTime) {
        assert sData != null;
        WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
        return (wm != null && wm.hasStartTime()) ? wm.getStartTime() : currentTime;
    }

    static void writeMultiWindowModeStartTime(int modeIndex, long startTime) {
        assert sData != null;
        WindowModeData wm =
                sData
                        .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                        .toBuilder()
                        .setStartTime(startTime)
                        .build();
        sData = sData.toBuilder().putWindowModes(modeIndex, wm).build();
        saveProto();
    }

    static long readMultiWindowModeDurationMs(int modeIndex) {
        assert sData != null;
        WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
        return windowModeData != null ? windowModeData.getDurationMs() : 0;
    }

    static void writeMultiWindowModeDurationMs(int modeIndex, long duration) {
        assert sData != null;
        WindowModeData windowModeData =
                sData
                        .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                        .toBuilder()
                        .setDurationMs(duration)
                        .build();
        sData = sData.toBuilder().putWindowModes(modeIndex, windowModeData).build();
        saveProto();
    }

    static Set<String> readMultiWindowModeActivities(int modeIndex) {
        assert sData != null;
        WindowModeData windowModeData = sData.getWindowModesMap().get(modeIndex);
        return (windowModeData != null && windowModeData.getActivitiesCount() > 0)
                ? Collections.unmodifiableSet(new HashSet<>(windowModeData.getActivitiesList()))
                : Collections.emptySet();
    }

    static void writeMultiWindowModeActivities(int modeIndex, Set<String> activities) {
        assert sData != null;
        WindowModeData windowModeData =
                sData
                        .getWindowModesOrDefault(modeIndex, WindowModeData.getDefaultInstance())
                        .toBuilder()
                        .clearActivities()
                        .addAllActivities(activities)
                        .build();
        sData = sData.toBuilder().putWindowModes(modeIndex, windowModeData).build();
        saveProto();
    }

    static void removeMultiWindowModeStartTime(int modeIndex) {
        assert sData != null;
        WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
        if (wm != null) {
            sData =
                    sData.toBuilder()
                            .putWindowModes(modeIndex, wm.toBuilder().clearStartTime().build())
                            .build();
            saveProto();
        }
    }

    static void removeMultiWindowModeDurationMs(int modeIndex) {
        assert sData != null;
        WindowModeData wm = sData.getWindowModesMap().get(modeIndex);
        if (wm != null) {
            sData =
                    sData.toBuilder()
                            .putWindowModes(modeIndex, wm.toBuilder().clearDurationMs().build())
                            .build();
            saveProto();
        }
    }

    public static void resetForTesting() {
        sData = null;
        sTaskRunner = null;

        // Delete the physical file so the next 'enabled' run starts fresh.
        if (sAtomicFile != null) {
            sAtomicFile.delete();
            sAtomicFile = null;
        }
    }
}
