// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.metrics;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.video_tutorials.FeatureType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Responsible for collecting metrics associated with video tutorials.
 */
public class VideoTutorialMetrics {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Must be kept in sync with VideoTutorials.WatchState in enums.xml.
    @IntDef({WatchState.STARTED, WatchState.COMPLETED, WatchState.PAUSED, WatchState.RESUMED,
            WatchState.WATCHED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface WatchState {
        int STARTED = 0;
        int COMPLETED = 1;
        int PAUSED = 2;
        int RESUMED = 3;
        int WATCHED = 4;
        int NUM_ENTRIES = 5;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Must be kept in sync with VideoTutorials.UserAction in enums.xml.
    @IntDef({UserAction.CHANGE_LANGUAGE, UserAction.WATCH_NEXT_VIDEO, UserAction.TRY_NOW,
            UserAction.SHARE, UserAction.CLOSE, UserAction.BACK_PRESS_WHEN_SHOWING_VIDEO_PLAYER,
            UserAction.OPEN_SHARED_VIDEO, UserAction.INVALID_SHARE_URL, UserAction.IPH_NTP_SHOWN,
            UserAction.IPH_NTP_CLICKED, UserAction.IPH_NTP_DISMISSED, UserAction.PLAYED_FROM_RECAP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UserAction {
        int CHANGE_LANGUAGE = 0;
        int WATCH_NEXT_VIDEO = 1;
        int TRY_NOW = 2;
        int SHARE = 3;
        int CLOSE = 4;
        int BACK_PRESS_WHEN_SHOWING_VIDEO_PLAYER = 5;
        int OPEN_SHARED_VIDEO = 6;
        int INVALID_SHARE_URL = 7;
        int IPH_NTP_SHOWN = 8;
        int IPH_NTP_CLICKED = 9;
        int IPH_NTP_DISMISSED = 10;
        int PLAYED_FROM_RECAP = 11;
        int NUM_ENTRIES = 12;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Must be kept in sync with VideoTutorials.LanguagePickerAction in
    // enums.xml.
    @IntDef({LanguagePickerAction.BACK_PRESS, LanguagePickerAction.CLOSE,
            LanguagePickerAction.WATCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LanguagePickerAction {
        int BACK_PRESS = 0;
        int CLOSE = 1;
        int WATCH = 2;
        int NUM_ENTRIES = 3;
    }

    /** Called to record various user actions on the video player. */
    public static void recordUserAction(@FeatureType int feature, @UserAction int action) {
        String histogramSuffix = histogramSuffixFromFeatureType(feature);
        RecordHistogram.recordEnumeratedHistogram(
                "VideoTutorials." + histogramSuffix + ".Player.UserAction", action,
                UserAction.NUM_ENTRIES);
    }

    public static void recordVideoLoadTimeLatency(long videoLoadTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "VideoTutorials.Player.LoadTimeLatency", videoLoadTime);
    }

    public static void recordWatchStateUpdate(@FeatureType int feature, @WatchState int state) {
        String histogramSuffix = histogramSuffixFromFeatureType(feature);
        RecordHistogram.recordEnumeratedHistogram(
                "VideoTutorials." + histogramSuffix + ".Player.Progress", state,
                WatchState.NUM_ENTRIES);
    }

    /** Called to record various user actions on the language picker. */
    public static void recordLanguagePickerAction(@LanguagePickerAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "VideoTutorials.LanguagePicker.Action", action, LanguagePickerAction.NUM_ENTRIES);
    }

    /**
     * Called when the user selects a language from the list of available languages. The position of
     * the language in the list will be recorded.
     * @param position The position of the language in the list.
     */
    public static void recordLanguageSelected(int position) {
        RecordHistogram.recordLinearCountHistogram(
                "VideoTutorials.LanguagePicker.LanguageSelected", position, 0, 20, 21);
    }

    private static String histogramSuffixFromFeatureType(@FeatureType int feature) {
        switch (feature) {
            case FeatureType.CHROME_INTRO:
                return "ChromeIntro";
            case FeatureType.DOWNLOAD:
                return "Download";
            case FeatureType.SEARCH:
                return "Search";
            case FeatureType.VOICE_SEARCH:
                return "VoiceSearch";
            case FeatureType.SUMMARY:
                return "Summary";
            default:
                return "Unknown";
        }
    }
}
