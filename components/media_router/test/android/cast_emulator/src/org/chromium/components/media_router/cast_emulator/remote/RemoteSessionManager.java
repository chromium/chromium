// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator.remote;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;

import androidx.mediarouter.media.MediaItemStatus;
import androidx.mediarouter.media.MediaSessionStatus;

import org.chromium.base.Log;

/**
 * RemoteSessionManager emulates the session management of the playback of media items on
 * Chromecast. This can be seen as emulating the cast receiver application. It only handles one item
 * at a time, and does not support queuing.
 *
 *  Actual playback of a single media item is abstracted into the DummyPlayer class, and is handled
 * outside this class.
 */
public class RemoteSessionManager implements DummyPlayer.Callback {
    private static final String TAG = "CastEmulator";

    /**
     * Connect a local session manager to the unique remote session manager, creating it if needed.
     * @param localSessionManager the local session manager being connected
     * @param player the player to use
     * @return the remote session manager
     */
    public static RemoteSessionManager connect(
            LocalSessionManager localSessionManager, Context context) {
        if (sInstance == null) {
            sInstance = new RemoteSessionManager("remote", context);
        }
        sInstance.mLocalSessionManager = localSessionManager;
        return sInstance;
    }

    private String mName;
    private int mSessionId;
    private int mItemId;
    private boolean mPaused;
    private boolean mSessionValid;
    private DummyPlayer mPlayer;
    private MediaItem mCurrentItem;

    @SuppressLint("StaticFieldLeak")
    private static RemoteSessionManager sInstance;

    private LocalSessionManager mLocalSessionManager;
    private Context mContext;

    private RemoteSessionManager(String name, Context context) {
        mName = name;
        mContext = context;
    }

    /**
     * Add a video we want to play
     *
     * @param uri the URI of the video
     * @param mime the mime type
     * @param receiver the pending intent to use to send state changes
     * @return the new media item
     */
    public MediaItem add(Uri uri, String mime, PendingIntent receiver, long contentPosition) {
        Log.v(TAG, "%s: add: uri=%s, receiver=%s, pos=%d", mName, uri, receiver, contentPosition);
        // create new session if needed
        startSession(false);
        checkPlayerAndSession();

        // create new item with initial status PLAYBACK_STATE_PENDING
        mItemId++;
        mCurrentItem =
                new MediaItem(
                        Integer.toString(mSessionId),
                        Integer.toString(mItemId),
                        uri,
                        mime,
                        receiver);
        mCurrentItem.setPosition(contentPosition);

        Log.v(TAG, "%s: add: new item id = %s", mName, mCurrentItem);
        return mCurrentItem;
    }

    /** Disconnect from the local session */
    public void disconnect() {
        mLocalSessionManager = null;
    }

    /**
     * Get the currently playing item
     *
     * @return the currently playing item, or null if none.
     */
    public MediaItem getCurrentItem() {
        return mCurrentItem;
    }

    /**
     * Get the session id of the current session
     *
     * @return the session id, or null if none.
     */
    public String getSessionId() {
        return mSessionValid ? Integer.toString(mSessionId) : null;
    }

    /**
     * Get the status of a session
     *
     * @param sid the session id of session being asked about
     * @return the status
     */
    public MediaSessionStatus getSessionStatus(String sid) {
        Log.v(TAG, "Getting session status for session %s", sid);
        int sessionState =
                (sid != null && sid.equals(Integer.toString(mSessionId)))
                        ? MediaSessionStatus.SESSION_STATE_ACTIVE
                        : MediaSessionStatus.SESSION_STATE_INVALIDATED;

        Log.v(TAG, "Session state is %s", sessionState);

        return new MediaSessionStatus.Builder(sessionState).setQueuePaused(mPaused).build();
    }

    /**
     * Get a printable string describing the status of the session
     * @return the string
     */
    public String getSessionStatusString() {
        if (mCurrentItem != null) {
            return "Current media item: " + mCurrentItem.toString();
        } else {
            return "No current media item";
        }
    }

    /**
     * Get the status of a media item
     *
     * @param iid - the id of the item
     * @return the MediaItem, from which its status can be read.
     */
    public MediaItem getStatus(String iid) {
        checkPlayerAndSession();
        checkItemCurrent(iid);

        mPlayer.getStatus(mCurrentItem, false);
        return mCurrentItem;
    }

    /** @return whether the current video is paused */
    public boolean isPaused() {
        return mSessionValid && mPaused;
    }

    @Override
    public void onCompletion() {
        finishItem(false);
    }

    // Player.Callback
    @Override
    public void onError() {
        finishItem(true);
    }

    @Override
    public void onSeekComplete() {
        // Playlist has changed, update the cached playlist
        updateStatus();
    }

    @Override
    public void onPrepared() {
        // Item is ready to play, update the status.
        updateStatus();
        // Send the new status to the local session manager.
        onItemChanged();
    }

    /** Pause the current video */
    public void pause() {
        Log.v(TAG, "%s: pause", mName);
        if (!mSessionValid) {
            return;
        }
        checkPlayer();
        mPaused = true;
        updatePlaybackState();
    }

    /** Resume the current video */
    public void resume() {
        Log.v(TAG, "%s: resume", mName);
        if (!mSessionValid) {
            return;
        }
        checkPlayer();
        mPaused = false;
        updatePlaybackState();
    }

    /**
     * Seek to a position in a video
     *
     * @param iid the id of the video
     * @param pos the position in ms
     * @return the Media item.
     */
    public MediaItem seek(String iid, long pos) {
        Log.v(TAG, "%s: seek: iid=%s, pos=%s", mName, iid, pos);
        checkPlayerAndSession();
        checkItemCurrent(iid);

        if (pos != mCurrentItem.getPosition()) {
            mCurrentItem.setPosition(pos);
            if (mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PLAYING
                    || mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PAUSED) {
                mPlayer.seek(mCurrentItem);
            }
        }
        return mCurrentItem;
    }

    /**
     * Start a new emulated Chromecast session if needed.
     *
     * @param relaunch relaunch the remote session (the emulation of the Chromecast app) even if it
     *        is already running.
     * @return The new session id
     */
    public String startSession(boolean relaunch) {
        if (!mSessionValid || relaunch) {
            if (mPlayer != null) mPlayer.setCallback(null);
            finishItem(false);
            if (mPlayer != null) mPlayer.release();
            mSessionId++;
            mItemId = 0;
            mPaused = false;
            mSessionValid = true;
            mPlayer = DummyPlayer.create(mContext, null);
            mPlayer.setCallback(this);
            mCurrentItem = null;
            Log.v(TAG, "Starting session %s", mSessionId);
        }
        return Integer.toString(mSessionId);
    }

    /** Stop the current video */
    public void stop() {
        Log.v(TAG, "%s: stop", mName);
        if (!mSessionValid) {
            return;
        }
        checkPlayer();
        mPlayer.stop();
        mCurrentItem = null;
        mPaused = false;
        updateStatus();
    }

    // Updates the playlist.
    public void updateStatus() {
        Log.v(TAG, "%s: updateStatus", mName);
        checkPlayer();

        if (mCurrentItem != null) {
            mPlayer.getStatus(mCurrentItem, /* update= */ true);
        }
    }

    private void checkItemCurrent(String iid) {
        if (mCurrentItem == null || !mCurrentItem.getItemId().equals(iid)) {
            throw new IllegalArgumentException("Item is not current!");
        }
    }

    private void checkPlayer() {
        if (mPlayer == null) {
            throw new IllegalStateException("Player not set!");
        }
    }

    private void checkPlayerAndSession() {
        checkPlayer();
        checkSession();
    }

    private void checkSession() {
        if (!mSessionValid) {
            throw new IllegalStateException("Session not set!");
        }
    }

    private void finishItem(boolean error) {
        if (mCurrentItem != null) {
            removeItem(
                    mCurrentItem.getItemId(),
                    error
                            ? MediaItemStatus.PLAYBACK_STATE_ERROR
                            : MediaItemStatus.PLAYBACK_STATE_FINISHED);
            updateStatus();
        }
    }

    private MediaItem removeItem(String iid, int state) {
        checkPlayerAndSession();

        checkItemCurrent(iid);

        if (mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PLAYING
                || mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PAUSED) {
            mPlayer.stop();
        }
        mCurrentItem.setState(state);
        onItemChanged();
        updatePlaybackState();

        MediaItem item = mCurrentItem;

        mCurrentItem = null;

        return item;
    }

    private void updatePlaybackState() {
        if (mCurrentItem != null) {
            if (mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PENDING) {
                mCurrentItem.setState(
                        mPaused
                                ? MediaItemStatus.PLAYBACK_STATE_PAUSED
                                : MediaItemStatus.PLAYBACK_STATE_PLAYING);
                mPlayer.play(mCurrentItem);
            } else if (mPaused
                    && mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PLAYING) {
                mPlayer.pause();
                mCurrentItem.setState(MediaItemStatus.PLAYBACK_STATE_PAUSED);
            } else if (!mPaused
                    && mCurrentItem.getState() == MediaItemStatus.PLAYBACK_STATE_PAUSED) {
                mPlayer.resume();
                mCurrentItem.setState(MediaItemStatus.PLAYBACK_STATE_PLAYING);
            }
            // notify client that item playback status has changed
            onItemChanged();
        }
        updateStatus();
    }

    private void onItemChanged() {
        if (mLocalSessionManager != null) {
            mLocalSessionManager.onItemChanged(mCurrentItem);
        }
    }
}
