// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.Set;

/**
 * Manages persisted multi-instance state. This includes information required to track metrics and
 * determine UI behavior.
 */
@NullMarked
public class MultiInstancePersistentStore {

    protected MultiInstancePersistentStore() {}

    static SharedPreferencesManager getManager() {
        return ChromeSharedPreferences.getInstance();
    }

    static long readMultiWindowStartTime() {
        return getManager().readLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME, 0);
    }

    static void writeMultiWindowStartTime(long startTime) {
        getManager().writeLong(ChromePreferenceKeys.MULTI_WINDOW_START_TIME, startTime);
    }

    static boolean readCloseWindowSkipConfirm() {
        return getManager()
                .readBoolean(ChromePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, false);
    }

    static void writeCloseWindowSkipConfirm(boolean skipConfirm) {
        getManager()
                .writeBoolean(
                        ChromePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, skipConfirm);
    }

    static int readMaxInstanceLimit(int maxInstance) {
        return getManager()
                .readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, maxInstance);
    }

    static void writeMaxInstanceLimit(int maxInstance) {
        getManager().writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT, maxInstance);
    }

    static boolean readInstanceLimitDowngradeTriggered() {
        return getManager()
                .readBoolean(
                        ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                        false);
    }

    static void writeInstanceLimitDowngradeTriggered(boolean triggered) {
        getManager()
                .writeBoolean(
                        ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                        triggered);
    }

    static boolean readRestorationMessageShown() {
        return getManager()
                .readBoolean(ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, false);
    }

    static void writeRestorationMessageShown(boolean triggered) {
        getManager()
                .writeBoolean(
                        ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN, triggered);
    }

    static long readMaxCountHistogramStartTime() {
        return getManager().readLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, 0);
    }

    static void writeMaxCountHistogramStartTime(long maxCountTime) {
        getManager().writeLong(ChromePreferenceKeys.MULTI_INSTANCE_MAX_COUNT_TIME, maxCountTime);
    }

    static int readDailyMaxActiveInstanceCount() {
        return getManager()
                .readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT, 0);
    }

    static void writeDailyMaxActiveInstanceCount(int count) {
        getManager().writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_ACTIVE_INSTANCE_COUNT, count);
    }

    static int readDailyMaxInstanceCount() {
        return getManager().readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, 0);
    }

    static void writeDailyMaxInstanceCount(int count) {
        getManager().writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT, count);
    }

    static int readDailyMaxIncognitoInstanceCount() {
        return getManager()
                .readInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO, 0);
    }

    static void writeDailyMaxIncognitoInstanceCount(int count) {
        getManager()
                .writeInt(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_COUNT_INCOGNITO, count);
    }

    static long readMultiInstanceStartTime() {
        return getManager().readLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, 0);
    }

    static void writeMultiInstanceStartTime(long startTime) {
        getManager().writeLong(ChromePreferenceKeys.MULTI_INSTANCE_START_TIME, startTime);
    }

    static long readMultiWindowModeCycleStartTime() {
        return getManager().readLong(ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, 0);
    }

    static void writeMultiWindowModeCycleStartTime(long startTime) {
        getManager().writeLong(ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, startTime);
    }

    static long readMultiWindowModeStartTime(int modeIndex, long currentTime) {
        String startTimeKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
        return getManager().readLong(startTimeKey, currentTime);
    }

    static void writeMultiWindowModeStartTime(int modeIndex, long startTime) {
        String startTimeKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
        getManager().writeLong(startTimeKey, startTime);
    }

    static long readMultiWindowModeDurationMs(int modeIndex) {
        String durationKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
        return getManager().readLong(durationKey, 0);
    }

    static void writeMultiWindowModeDurationMs(int modeIndex, long startTime) {
        String durationKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
        getManager().writeLong(durationKey, startTime);
    }

    static @Nullable Set<String> readMultiWindowModeActivities(int modeIndex) {
        String activitiesKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(modeIndex);
        return getManager().readStringSet(activitiesKey, null);
    }

    static void writeMultiWindowModeActivities(int modeIndex, Set<String> startTime) {
        String startTimeKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(modeIndex);
        getManager().writeStringSet(startTimeKey, startTime);
    }

    static void removeMultiWindowModeStartTime(int modeIndex) {
        String startTimeKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
        getManager().removeKey(startTimeKey);
    }

    static void removeMultiWindowModeDurationMs(int modeIndex) {
        String durationKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
        getManager().removeKey(durationKey);
    }

    static boolean contains(String key) {
        return getManager().contains(key);
    }
}
