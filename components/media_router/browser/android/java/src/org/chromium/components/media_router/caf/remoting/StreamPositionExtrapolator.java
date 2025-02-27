// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

/**
 * Class for extrapolating current playback position. The class occasionally receives updated
 * playback position information from RemoteMediaClient, and extrapolates the current playback
 * position.
 */
public class StreamPositionExtrapolator {
    private long mDuration;
    private long mLastKnownPosition;
    private long mTimestamp;
    private boolean mIsPlaying;
    private double mPlaybackRate;

    public StreamPositionExtrapolator() {
        clear();
    }

    public void clear() {
        mDuration = 0;
        mLastKnownPosition = 0;
        mTimestamp = 0;
        mIsPlaying = false;
        mPlaybackRate = 1.0;
    }

    /** Updates the extrapolator with latest playback state. */
    public void update(long duration, long position, boolean isPlaying, double playbackRate) {
        mDuration = duration;
        mLastKnownPosition = position;
        mIsPlaying = isPlaying;
        mPlaybackRate = playbackRate;
        mTimestamp = System.currentTimeMillis();
    }

    /** Called when the remote media has finished. */
    public void onFinish() {
        mIsPlaying = false;
        mLastKnownPosition = mDuration;
        mTimestamp = System.currentTimeMillis();
    }

    /** Called when a seek command is sent out. */
    public void onSeek(long position) {
        mIsPlaying = false;
        mLastKnownPosition = position;
        mTimestamp = System.currentTimeMillis();
    }

    /** Returns the approximate position. */
    public long getPosition() {
        if (mTimestamp == 0) return 0;
        if (!mIsPlaying) return Math.max(mLastKnownPosition, 0);

        long interpolatedStreamPosition =
                mLastKnownPosition
                        + (long) (mPlaybackRate * (System.currentTimeMillis() - mTimestamp));
        if (mDuration >= 0) {
            // Don't limit if mDuration is negative, which means the remote media is streamed
            // instead of buffered.
            interpolatedStreamPosition = Math.min(interpolatedStreamPosition, mDuration);
        }
        return Math.max(interpolatedStreamPosition, 0);
    }

    /** Returns the stream duration. */
    public long getDuration() {
        return mDuration;
    }
}
