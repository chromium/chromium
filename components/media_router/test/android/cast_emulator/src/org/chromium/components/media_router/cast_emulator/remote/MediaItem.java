// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator.remote;

import android.app.PendingIntent;
import android.net.Uri;
import android.os.SystemClock;

import androidx.mediarouter.media.MediaItemStatus;

import org.chromium.base.Log;

/** PlaylistItem helps keep track of the current status of an media item. */
final class MediaItem {
    // immutables
    private final String mSessionId;
    private final String mItemId;
    private final Uri mUri;
    private final PendingIntent mUpdateReceiver;
    // changeable states
    private int mPlaybackState = MediaItemStatus.PLAYBACK_STATE_PENDING;
    private long mContentPosition;
    private long mContentDuration;
    private long mTimestamp;
    private String mRemoteItemId;
    private static final String TAG = "CastEmulator";

    public MediaItem(String qid, String iid, Uri uri, String mime, PendingIntent pi) {
        mSessionId = qid;
        mItemId = iid;
        mUri = uri;
        mUpdateReceiver = pi;
        setTimestamp(SystemClock.elapsedRealtime());
    }

    public void setRemoteItemId(String riid) {
        mRemoteItemId = riid;
    }

    public void setState(int state) {
        mPlaybackState = state;
        Log.v(TAG, "State set to %d", state);
    }

    public void setPosition(long pos) {
        mContentPosition = pos;
    }

    public void setTimestamp(long ts) {
        mTimestamp = ts;
    }

    public void setDuration(long duration) {
        mContentDuration = duration;
    }

    public String getSessionId() {
        return mSessionId;
    }

    public String getItemId() {
        return mItemId;
    }

    public String getRemoteItemId() {
        return mRemoteItemId;
    }

    public Uri getUri() {
        return mUri;
    }

    public PendingIntent getUpdateReceiver() {
        return mUpdateReceiver;
    }

    public int getState() {
        return mPlaybackState;
    }

    public long getPosition() {
        return mContentPosition;
    }

    public long getDuration() {
        return mContentDuration;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public MediaItemStatus getStatus() {
        return new MediaItemStatus.Builder(mPlaybackState)
                .setContentPosition(mContentPosition)
                .setContentDuration(mContentDuration)
                .setTimestamp(mTimestamp)
                .build();
    }

    @Override
    public String toString() {
        String state[] = {
            "PENDING",
            "PLAYING",
            "PAUSED",
            "BUFFERING",
            "FINISHED",
            "CANCELED",
            "INVALIDATED",
            "ERROR"
        };
        return "["
                + mSessionId
                + "|"
                + mItemId
                + "|"
                + (mRemoteItemId != null ? mRemoteItemId : "-")
                + "|"
                + state[mPlaybackState]
                + "] "
                + mUri.toString();
    }
}
