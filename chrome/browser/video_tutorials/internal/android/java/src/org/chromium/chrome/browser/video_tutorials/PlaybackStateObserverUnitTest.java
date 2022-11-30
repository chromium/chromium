// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.services.media_session.MediaPosition;

/**
 * Tests for {@link PlaybackStateObserver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PlaybackStateObserverUnitTest {
    private static final long DURATION_MS = 10000L;

    @Mock
    private MediaSession mMediaSession;
    @Mock
    private PlaybackStateObserver.Observer mObserver;

    private PlaybackStateObserver mPlaybackStateObserver;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);

        mPlaybackStateObserver =
                new PlaybackStateObserver(mMediaSession, () -> { return mObserver; });
    }

    @Test
    public void testInitialState() {
        WatchStateInfo info = mPlaybackStateObserver.getWatchStateInfo();
        assertEquals(State.INITIAL, info.state);
    }

    @Test
    public void testPlaybackStarted() {
        MediaPosition position = new MediaPosition(DURATION_MS, 0, 1.f, 2);
        mPlaybackStateObserver.mediaSessionStateChanged(true, false);
        mPlaybackStateObserver.mediaSessionPositionChanged(position);
        WatchStateInfo info = mPlaybackStateObserver.getWatchStateInfo();
        assertEquals(State.PLAYING, info.state);
        Mockito.verify(mObserver).onPlay();
    }

    @Test
    public void testPlaybackPaused() {
        MediaPosition position = new MediaPosition(DURATION_MS, 100, 1.f, 2);
        mPlaybackStateObserver.mediaSessionStateChanged(true, true);
        mPlaybackStateObserver.mediaSessionPositionChanged(position);
        WatchStateInfo info = mPlaybackStateObserver.getWatchStateInfo();
        assertEquals(State.PAUSED, info.state);
        Mockito.verify(mObserver).onPause();
    }

    @Test
    public void testPlaybackComplete() {
        mPlaybackStateObserver.reset();

        long initialSystemTime = 30000L;
        PlaybackStateObserver.setCurrentSystemTimeForTesting(initialSystemTime);
        MediaPosition position = new MediaPosition(DURATION_MS, 0, 1.f, initialSystemTime);
        mPlaybackStateObserver.mediaSessionStateChanged(true, false);
        mPlaybackStateObserver.mediaSessionPositionChanged(position);
        verifyWatchState(State.PLAYING, false, DURATION_MS, 0);
        Mockito.verify(mObserver).onPlay();

        // Slightly advance the playback to 100ms.
        PlaybackStateObserver.setCurrentSystemTimeForTesting(initialSystemTime + 100);
        MediaPosition position2 = new MediaPosition(DURATION_MS, 100, 1.f, initialSystemTime + 100);
        mPlaybackStateObserver.mediaSessionStateChanged(true, false);
        mPlaybackStateObserver.mediaSessionPositionChanged(position2);
        Mockito.verify(mObserver).onPlay();

        // Advance playback to almost completion with 100ms remaining.
        PlaybackStateObserver.setCurrentSystemTimeForTesting(initialSystemTime + DURATION_MS - 100);
        MediaPosition position3 = new MediaPosition(
                DURATION_MS, DURATION_MS - 100, 1.f, initialSystemTime + DURATION_MS - 100);
        mPlaybackStateObserver.mediaSessionStateChanged(true, false);
        mPlaybackStateObserver.mediaSessionPositionChanged(position3);
        Mockito.verify(mObserver).onPlay();

        // Complete the video.
        PlaybackStateObserver.setCurrentSystemTimeForTesting(initialSystemTime + DURATION_MS);
        mPlaybackStateObserver.mediaSessionStateChanged(false, false);
        mPlaybackStateObserver.mediaSessionPositionChanged(null);
        verifyWatchState(State.ENDED, true, DURATION_MS, DURATION_MS);
        Mockito.verify(mObserver).onEnded();
    }

    private void verifyWatchState(
            State state, boolean videoWatched, long duration, long currentPosition) {
        assertEquals(state, mPlaybackStateObserver.getWatchStateInfo().state);
        assertEquals(duration, mPlaybackStateObserver.getWatchStateInfo().videoLength);
        assertEquals(currentPosition, mPlaybackStateObserver.getWatchStateInfo().currentPosition);
        assertEquals(videoWatched, mPlaybackStateObserver.getWatchStateInfo().videoWatched());
    }
}
