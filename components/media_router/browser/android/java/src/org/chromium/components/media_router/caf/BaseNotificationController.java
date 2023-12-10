// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.content.Intent;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationListener;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.R;
import org.chromium.services.media_session.MediaMetadata;

/** Base controller for updating media notification for Casting and MediaFling. */
public abstract class BaseNotificationController
        implements MediaNotificationListener, BaseSessionController.Callback {
    private MediaNotificationInfo.Builder mNotificationBuilder;
    private final BaseSessionController mSessionController;

    public BaseNotificationController(BaseSessionController sessionController) {
        assert MediaRouterClient.getInstance() != null;
        mSessionController = sessionController;
    }

    @Override
    public void onSessionStarted() {
        mNotificationBuilder =
                new MediaNotificationInfo.Builder()
                        .setPaused(false)
                        .setOrigin(mSessionController.getRouteCreationInfo().origin)
                        // TODO(zqzhang): the same session might have more than one tab id. Should
                        // we track the last foreground alive tab and update the notification with
                        // it?
                        .setInstanceId(mSessionController.getRouteCreationInfo().tabId)
                        .setPrivate(mSessionController.getRouteCreationInfo().isOffTheRecord)
                        .setActions(MediaNotificationInfo.ACTION_STOP)
                        .setContentIntent(createContentIntent())
                        .setNotificationSmallIcon(R.drawable.ic_notification_media_route)
                        .setDefaultNotificationLargeIcon(R.drawable.cast_playing_square)
                        .setId(getNotificationId())
                        .setListener(this);

        updateNotificationMetadata();
        MediaRouterClient.getInstance().showNotification(mNotificationBuilder.build());
    }

    @Override
    public void onSessionEnded() {
        MediaNotificationManager.clear(getNotificationId());
        mNotificationBuilder = null;
    }

    /** Called when media status updated. */
    @Override
    public void onStatusUpdated() {
        if (mNotificationBuilder == null) return;
        if (!mSessionController.isConnected()) return;

        MediaStatus mediaStatus = mSessionController.getRemoteMediaClient().getMediaStatus();
        if (mediaStatus == null) return;

        int playerState = mediaStatus.getPlayerState();
        if (playerState == MediaStatus.PLAYER_STATE_PAUSED
                || playerState == MediaStatus.PLAYER_STATE_PLAYING) {
            mNotificationBuilder.setPaused(playerState != MediaStatus.PLAYER_STATE_PLAYING);
            mNotificationBuilder.setActions(
                    MediaNotificationInfo.ACTION_STOP | MediaNotificationInfo.ACTION_PLAY_PAUSE);
        } else {
            mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP);
        }
        MediaRouterClient.getInstance().showNotification(mNotificationBuilder.build());
    }

    /** Called when media metadata updated. */
    @Override
    public void onMetadataUpdated() {
        if (mNotificationBuilder == null) return;
        updateNotificationMetadata();
        MediaRouterClient.getInstance().showNotification(mNotificationBuilder.build());
    }

    private void updateNotificationMetadata() {
        // We use a placeholder title here to comply with the requirement for non-empty
        // notification titles. See crbug.com/1445673 for more details.
        MediaMetadata notificationMetadata = new MediaMetadata("Chromecast", "", "");
        mNotificationBuilder.setMetadata(notificationMetadata);

        if (!mSessionController.isConnected()) return;

        CastDevice castDevice = mSessionController.getSession().getCastDevice();
        if (castDevice != null) {
            String friendlyName = castDevice.getFriendlyName();
            if (friendlyName != null && !friendlyName.isEmpty()) {
                notificationMetadata.setTitle(friendlyName);
            }
        }

        RemoteMediaClient remoteMediaClient = mSessionController.getRemoteMediaClient();

        com.google.android.gms.cast.MediaInfo info = remoteMediaClient.getMediaInfo();
        if (info == null) return;

        com.google.android.gms.cast.MediaMetadata metadata = info.getMetadata();
        if (metadata == null) return;

        String title = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_TITLE);
        if (title != null && !title.isEmpty()) notificationMetadata.setTitle(title);

        String artist = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ARTIST);
        if (artist == null) {
            artist = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ALBUM_ARTIST);
        }
        if (artist != null) notificationMetadata.setArtist(artist);

        String album =
                metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ALBUM_TITLE);
        if (album != null) notificationMetadata.setAlbum(album);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // MediaNotificationListener implementation.

    @Override
    public void onPlay(int actionSource) {
        if (!mSessionController.isConnected()) return;

        mSessionController.getRemoteMediaClient().play();
    }

    @Override
    public void onPause(int actionSource) {
        if (!mSessionController.isConnected()) return;

        mSessionController.getRemoteMediaClient().pause();
    }

    @Override
    public void onStop(int actionSource) {
        if (!mSessionController.isConnected()) return;

        mSessionController.endSession();
    }

    @Override
    public void onMediaSessionAction(int action) {}

    @Override
    public void onMediaSessionSeekTo(long pos) {}

    protected Intent createBringTabToFrontIntent() {
        return MediaRouterClient.getInstance()
                .createBringTabToFrontIntent(mSessionController.getRouteCreationInfo().tabId);
    }

    // Abstract methods to be implemented by children.
    public abstract Intent createContentIntent();

    public abstract int getNotificationId();
}
