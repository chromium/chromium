// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.services.media_session.MediaPosition;

/**
 * Responsible for observing a media session and notifying the observer about play/pause/end media
 * events.
 */
public class PlaybackStateObserver extends MediaSessionObserver {
    /**
     * A ratio used for collecting metrics used to determine whether a video was sufficiently
     * watched by the user.
     */
    private static final float WATCH_COMPLETION_RATIO_THRESHOLD = 0.5f;

    /** Contains playback info about currently playing media. */
    public static class WatchStateInfo {
        /** Contains various states during the media playback. */
        public enum State {
            INITIAL,
            PLAYING,
            PAUSED,
            ENDED,
            ERROR,
        }

        /** The current state. */
        public State state = State.INITIAL;

        /** The duration of the video. */
        public long videoLength;

        /** The current position of the video. */
        public long currentPosition;

        /**
         * Whether the video has been watched up to a certain point so that it can be considered as
         * completed.
         */
        public boolean videoWatched() {
            return currentPosition > videoLength * WATCH_COMPLETION_RATIO_THRESHOLD;
        }
    }

    /**
     * Interface to be notified of playback state updates.
     */
    public interface Observer {
        /** Called when the player has started playing or resumed. */
        void onPlay();

        /** Called when the player has been paused. */
        void onPause();

        /** Called when the player has completed playing the video. */
        void onEnded();

        /** Called when an error has occurred. */
        void onError();
    }

    private static Long sCurrentSystemTimeForTesting;
    private final Supplier<Observer> mObserver;
    private WatchStateInfo mWatchStateInfo = new WatchStateInfo();
    private MediaPosition mLastPosition;
    private boolean mIsControllable;
    private boolean mIsSuspended;

    /** Constructor. */
    public PlaybackStateObserver(MediaSession mediaSession, Supplier<Observer> observer) {
        super(mediaSession);
        mObserver = observer;
    }

    /**
     * Called to get the current media playback info, such as duration, current progress, playback
     * state etc.
     * @return The current watch state info.
     */
    public WatchStateInfo getWatchStateInfo() {
        return mWatchStateInfo;
    }

    /** Reset internal state. */
    public void reset() {
        mLastPosition = null;
        mIsControllable = false;
        mIsSuspended = false;
        mWatchStateInfo = new WatchStateInfo();
    }

    @Override
    public void mediaSessionPositionChanged(MediaPosition position) {
        if (position != null) {
            mWatchStateInfo.videoLength = position.getDuration();
        }

        updateState(position);
        mWatchStateInfo.currentPosition =
                computeCurrentPosition(position == null ? mLastPosition : position);
        mLastPosition = position;
    }

    @Override
    public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        mIsControllable = isControllable;
        mIsSuspended = isSuspended;
        updateState(mLastPosition);
    }

    private void updateState(MediaPosition newPosition) {
        State nextState = mWatchStateInfo.state;
        if (mIsControllable) {
            nextState = mIsSuspended ? State.PAUSED : State.PLAYING;
        } else if (newPosition == null) {
            // TODO(shaktisahu): Determine error state.
            if (mLastPosition == null) {
                nextState = State.INITIAL;
            } else if (mLastPosition.getDuration() == computeCurrentPosition(mLastPosition)) {
                nextState = State.ENDED;
            }
        }

        updateObservers(nextState);
    }

    private void updateObservers(State nextState) {
        if (nextState == mWatchStateInfo.state) return;

        mWatchStateInfo.state = nextState;
        switch (nextState) {
            case INITIAL:
                break;
            case PLAYING:
                mObserver.get().onPlay();
                break;
            case PAUSED:
                mObserver.get().onPause();
                break;
            case ENDED:
                mObserver.get().onEnded();
                break;
            case ERROR:
                mObserver.get().onError();
                break;
            default:
                assert false : "Unknown media playback state";
        }
    }

    private static long computeCurrentPosition(MediaPosition mediaPosition) {
        if (mediaPosition == null) return 0;

        long elapsedTime = getCurrentSystemTime() - mediaPosition.getLastUpdatedTime();
        long updatedPosition = (long) (mediaPosition.getPosition()
                + (elapsedTime * mediaPosition.getPlaybackRate()));
        updatedPosition = Math.min(updatedPosition, mediaPosition.getDuration());
        if (mediaPosition.getDuration() - updatedPosition < 100) {
            updatedPosition = mediaPosition.getDuration();
        }
        return updatedPosition;
    }

    private static long getCurrentSystemTime() {
        if (sCurrentSystemTimeForTesting != null) return sCurrentSystemTimeForTesting;
        return SystemClock.elapsedRealtime();
    }

    @VisibleForTesting
    protected static void setCurrentSystemTimeForTesting(Long currentTimeForTesting) {
        sCurrentSystemTimeForTesting = currentTimeForTesting;
    }
}
