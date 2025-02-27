// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import com.google.android.gms.cast.MediaInfo;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;
import com.google.android.gms.common.api.Result;

import org.chromium.base.Log;
import org.chromium.components.media_router.FlingingController;
import org.chromium.components.media_router.MediaController;
import org.chromium.components.media_router.MediaStatusBridge;
import org.chromium.components.media_router.MediaStatusObserver;

/** Adapter class for bridging {@link RemoteMediaClient} and {@link FlingController}. */
public class FlingingControllerAdapter implements FlingingController, MediaController {
    private static final String TAG = "FlingCtrlAdptr";

    private final StreamPositionExtrapolator mStreamPositionExtrapolator;
    private final RemotingSessionController mSessionController;
    private String mMediaUrl;
    private MediaStatusObserver mMediaStatusObserver;
    private boolean mLoaded;
    private boolean mHasEverReceivedValidMediaSession;

    FlingingControllerAdapter(RemotingSessionController sessionController, String mediaUrl) {
        mSessionController = sessionController;
        mMediaUrl = mediaUrl;
        mStreamPositionExtrapolator = new StreamPositionExtrapolator();
    }

    /**
     * Called when media source needs to be updated.
     *
     * @param mediaUrl The new media source URL.
     */
    public void updateMediaUrl(String mediaUrl) {
        mMediaUrl = mediaUrl;
        mLoaded = false;
        load(/* position= */ 0, /* autoplay= */ false);
    }

    ////////////////////////////////////////////
    // FlingingController implementation begin
    ////////////////////////////////////////////

    @Override
    public MediaController getMediaController() {
        return this;
    }

    @Override
    public void setMediaStatusObserver(MediaStatusObserver observer) {
        assert mMediaStatusObserver == null;
        mMediaStatusObserver = observer;
    }

    @Override
    public void clearMediaStatusObserver() {
        assert mMediaStatusObserver != null;
        mMediaStatusObserver = null;
    }

    @Override
    public long getApproximateCurrentTime() {
        return mStreamPositionExtrapolator.getPosition();
    }

    public long getDuration() {
        return mStreamPositionExtrapolator.getDuration();
    }

    ////////////////////////////////////////////
    // FlingingController implementation end
    ////////////////////////////////////////////

    /** Starts loading the media URL, from the given position. */
    public void load(long position, boolean autoplay) {
        if (!mSessionController.isConnected()) return;

        mLoaded = true;

        MediaInfo mediaInfo =
                new MediaInfo.Builder(mMediaUrl)
                        .setContentType("*/*")
                        .setStreamType(MediaInfo.STREAM_TYPE_BUFFERED)
                        .build();
        mSessionController.getRemoteMediaClient().load(mediaInfo, autoplay, position);
    }

    ////////////////////////////////////////////
    // MediaController implementation begin
    ////////////////////////////////////////////

    @Override
    public void play() {
        if (!mSessionController.isConnected()) return;

        if (!mLoaded) {
            load(/* position= */ 0, /* autoplay= */ true);
            return;
        }

        mSessionController
                .getRemoteMediaClient()
                .play()
                .setResultCallback(this::onMediaCommandResult);
    }

    @Override
    public void pause() {
        if (!mSessionController.isConnected()) return;
        mSessionController
                .getRemoteMediaClient()
                .pause()
                .setResultCallback(this::onMediaCommandResult);
    }

    @Override
    public void setMute(boolean mute) {
        if (!mSessionController.isConnected()) return;
        mSessionController
                .getRemoteMediaClient()
                .setStreamMute(mute)
                .setResultCallback(this::onMediaCommandResult);
    }

    @Override
    public void setVolume(double volume) {
        if (!mSessionController.isConnected()) return;
        mSessionController
                .getRemoteMediaClient()
                .setStreamVolume(volume)
                .setResultCallback(this::onMediaCommandResult);
    }

    @Override
    public void seek(long position) {
        if (!mSessionController.isConnected()) return;

        if (!mLoaded) {
            load(position, /* autoplay= */ true);
            return;
        }

        mSessionController
                .getRemoteMediaClient()
                .seek(position)
                .setResultCallback(this::onMediaCommandResult);
        mStreamPositionExtrapolator.onSeek(position);
    }

    ////////////////////////////////////////////
    // MediaController implementation end
    ////////////////////////////////////////////

    public void onStatusUpdated() {
        if (mMediaStatusObserver == null) return;

        RemoteMediaClient remoteMediaClient = mSessionController.getRemoteMediaClient();

        MediaStatus mediaStatus = remoteMediaClient.getMediaStatus();
        if (mediaStatus != null) {
            mHasEverReceivedValidMediaSession = true;
            if (mediaStatus.getPlayerState() == MediaStatus.PLAYER_STATE_IDLE
                    && mediaStatus.getIdleReason() == MediaStatus.IDLE_REASON_FINISHED) {
                mLoaded = false;
                mStreamPositionExtrapolator.onFinish();
            } else {
                mStreamPositionExtrapolator.update(
                        remoteMediaClient.getStreamDuration(),
                        remoteMediaClient.getApproximateStreamPosition(),
                        remoteMediaClient.isPlaying(),
                        mediaStatus.getPlaybackRate());
            }

            mMediaStatusObserver.onMediaStatusUpdate(new MediaStatusBridge(mediaStatus));

        } else if (mHasEverReceivedValidMediaSession) {
            // We can receive a null |mediaStatus| while we are in the process of loading the video.
            // We should wait until we receive one valid media status before considering the video
            // unloaded. Otherwise, the first call to seek or play will reload the video.
            // See b/144325733.
            mLoaded = false;
            mStreamPositionExtrapolator.clear();
        }
    }

    private void onMediaCommandResult(Result result) {
        // When multiple API calls are made in quick succession, "Results have already been set"
        // IllegalStateExceptions might be thrown from GMS code. We prefer to catch the exception
        // and noop it, than to crash. This might lead to some API calls never getting their
        // onResult() called, so we should not rely on onResult() being called for every API call.
        // See https://crbug.com/853923.
        if (!result.getStatus().isSuccess()) {
            Log.e(
                    TAG,
                    "Error when sending command. Status code: %d",
                    result.getStatus().getStatusCode());
        }
    }
}
