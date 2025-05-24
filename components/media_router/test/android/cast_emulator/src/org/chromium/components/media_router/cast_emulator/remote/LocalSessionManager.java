// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator.remote;

import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;

import androidx.mediarouter.media.MediaSessionStatus;

/**
 * LocalSessionManager emulates the local management of the playback of media items on Chromecast.
 * It only handles one item at a time, and does not support queuing.
 *
 *  Most members simply forward their calls to a RemoteSessionManager, which emulates the session
 * management on the Chromecast, however this class also controls connection to and disconnection
 * from the RemoteSessionManager.
 */
public class LocalSessionManager {
    /** Callbacks for MediaRouteProvider object. */
    public interface Callback {
        void onItemChanged(MediaItem item);
    }

    private Callback mCallback;
    private RemoteSessionManager mRemoteManager;

    private final Context mContext;

    public LocalSessionManager(Context context) {
        mContext = context;
    }

    /**
     * Add a video we want to play
     * @param uri the URI of the video
     * @param mime the mime type
     * @param receiver the pending intent to use to send state changes
     * @return the new media item
     */
    public MediaItem add(Uri uri, String mime, PendingIntent receiver, long contentPosition) {
        if (!hasSession()) mRemoteManager = RemoteSessionManager.connect(this, mContext);
        return mRemoteManager.add(uri, mime, receiver, contentPosition);
    }

    /**
     * End the current session
     * @return whether there was a current session
     */
    public boolean endSession() {
        if (hasSession()) {
            mRemoteManager.disconnect();
            mRemoteManager = null;
            return true;
        }
        return false;
    }

    /**
     * Get the currently playing item
     * @return the currently playing item, or null if none.
     */
    public MediaItem getCurrentItem() {
        return hasSession() ? mRemoteManager.getCurrentItem() : null;
    }

    /**
     * Get the session id of the current session
     * @return the session id, or null if none.
     */
    public String getSessionId() {
        return hasSession() ? mRemoteManager.getSessionId() : null;
    }

    /**
     * Get the status of a session
     * @param sid the session id of session being asked about
     * @return the status
     */
    public MediaSessionStatus getSessionStatus(String sid) {
        if (!hasSession()) {
            return new MediaSessionStatus.Builder(MediaSessionStatus.SESSION_STATE_INVALIDATED)
                    .setQueuePaused(false)
                    .build();
        }
        return mRemoteManager.getSessionStatus(sid);
    }

    /**
     * Get a printable string describing the status of the session
     * @return the string
     */
    public String getSessionStatusString() {
        if (hasSession()) {
            return mRemoteManager.getSessionStatusString();
        } else {
            return "No remote session connection";
        }
    }

    /**
     * Get the status of a media item
     * @param iid - the id of the item
     * @return the MediaItem, from which its status can be read.
     */
    public MediaItem getStatus(String iid) {
        if (!hasSession()) {
            throw new IllegalStateException("Session not set!");
        }
        return mRemoteManager.getStatus(iid);
    }

    /** @return whether there is a current session */
    public boolean hasSession() {
        return mRemoteManager != null;
    }

    /** @return whether the current video is paused */
    public boolean isPaused() {
        return hasSession() && mRemoteManager.isPaused();
    }

    /**
     * Forward the item changed callback to the UI
     * @param item the item that has changed.
     */
    public void onItemChanged(MediaItem item) {
        if (mCallback != null) mCallback.onItemChanged(item);
    }

    /** Pause the current video */
    public void pause() {
        if (hasSession()) mRemoteManager.pause();
    }

    /** Resume the current video */
    public void resume() {
        if (hasSession()) mRemoteManager.resume();
    }

    /**
     * Seek to a position in a video
     * @param iid the id of the video
     * @param pos the position in ms
     * @return the Media item.
     */
    public MediaItem seek(String iid, long pos) {
        return hasSession() ? mRemoteManager.seek(iid, pos) : null;
    }

    /**
     * provide a callback interface to tell the UI when significant state changes occur
     * @param callback the callback object
     */
    public void setCallback(Callback callback) {
        mCallback = callback;
    }

    /**
     * Start a new local session
     * @param relaunch relaunch the remote session (the emulation of the Chromecast app) even if it
     *        is already running.
     * @return The new session id
     */
    public String startSession(boolean relaunch) {
        if (!relaunch) endSession();
        if (!hasSession()) mRemoteManager = RemoteSessionManager.connect(this, mContext);
        return mRemoteManager.startSession(relaunch);
    }

    /** Stop the current video */
    public void stop() {
        if (hasSession()) mRemoteManager.stop();
        endSession();
    }

    /** Updates the session status. */
    public void updateStatus() {
        if (hasSession()) mRemoteManager.updateStatus();
    }
}
