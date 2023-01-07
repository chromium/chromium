// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.metrics;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.UserAction;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.WatchState;

/**
 * Tests for {@link VideoTutorialMetrics}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class VideoTutorialMetricsUnitTest {
    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    public void testUserActionHistogramNames() {
        VideoTutorialMetrics.recordUserAction(FeatureType.CHROME_INTRO, UserAction.SHARE);
        VideoTutorialMetrics.recordUserAction(FeatureType.DOWNLOAD, UserAction.CLOSE);
        VideoTutorialMetrics.recordUserAction(FeatureType.SEARCH, UserAction.CHANGE_LANGUAGE);
        VideoTutorialMetrics.recordUserAction(
                FeatureType.VOICE_SEARCH, UserAction.WATCH_NEXT_VIDEO);
        VideoTutorialMetrics.recordUserAction(FeatureType.SEARCH, UserAction.TRY_NOW);
        VideoTutorialMetrics.recordUserAction(
                FeatureType.VOICE_SEARCH, UserAction.BACK_PRESS_WHEN_SHOWING_VIDEO_PLAYER);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.UserAction", UserAction.SHARE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.Download.Player.UserAction", UserAction.CLOSE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.Search.Player.UserAction", UserAction.CHANGE_LANGUAGE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.VoiceSearch.Player.UserAction",
                        UserAction.WATCH_NEXT_VIDEO));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.Search.Player.UserAction", UserAction.TRY_NOW));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.VoiceSearch.Player.UserAction",
                        UserAction.BACK_PRESS_WHEN_SHOWING_VIDEO_PLAYER));
    }

    @Test
    public void testWatchStateHistogramNames() {
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.STARTED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.COMPLETED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.PAUSED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.RESUMED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.PAUSED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.RESUMED);
        VideoTutorialMetrics.recordWatchStateUpdate(FeatureType.CHROME_INTRO, WatchState.WATCHED);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.Progress", WatchState.STARTED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.Progress", WatchState.COMPLETED));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.Progress", WatchState.PAUSED));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.Progress", WatchState.RESUMED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VideoTutorials.ChromeIntro.Player.Progress", WatchState.WATCHED));
    }
}
