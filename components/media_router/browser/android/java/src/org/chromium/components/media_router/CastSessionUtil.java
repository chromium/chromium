// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.RemoteMediaPlayer;

import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.services.media_session.MediaMetadata;

/**
 * Helper class that implements functions useful to all CastSession types.
 */
public class CastSessionUtil {
    public static final String MEDIA_NAMESPACE = "urn:x-cast:com.google.cast.media";

    // The value is borrowed from the Android Cast SDK code to match their behavior.
    public static final double MIN_VOLUME_LEVEL_DELTA = 1e-7;

    /**
     * Builds a MediaMetadata from the given CastDevice and MediaPlayer, and sets it on the builder
     */
    public static void setNotificationMetadata(MediaNotificationInfo.Builder builder,
            CastDevice castDevice, RemoteMediaPlayer mediaPlayer) {
        MediaMetadata notificationMetadata = new MediaMetadata("", "", "");
        builder.setMetadata(notificationMetadata);

        if (castDevice != null) notificationMetadata.setTitle(castDevice.getFriendlyName());

        if (mediaPlayer == null) return;

        com.google.android.gms.cast.MediaInfo info = mediaPlayer.getMediaInfo();
        if (info == null) return;

        com.google.android.gms.cast.MediaMetadata metadata = info.getMetadata();
        if (metadata == null) return;

        String title = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_TITLE);
        if (title != null) notificationMetadata.setTitle(title);

        String artist = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ARTIST);
        if (artist == null) {
            artist = metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ALBUM_ARTIST);
        }
        if (artist != null) notificationMetadata.setArtist(artist);

        String album =
                metadata.getString(com.google.android.gms.cast.MediaMetadata.KEY_ALBUM_TITLE);
        if (album != null) notificationMetadata.setAlbum(album);
    }
}
