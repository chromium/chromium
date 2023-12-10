// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

/** Interface for classes that need to be notified about media events. */
public interface MediaNotificationListener {
    /** The media action was caused by direct interaction with the notification. */
    public static final int ACTION_SOURCE_MEDIA_NOTIFICATION = 1000;

    /** The media action was received via the MediaSession Android API, e.g. a headset, a watch, etc. */
    public static final int ACTION_SOURCE_MEDIA_SESSION = 1001;

    /**
     * The media action was received by unplugging the headset,
     * which broadcasts an ACTION_AUDIO_BECOMING_NOISY intent.
     */
    public static final int ACTION_SOURCE_HEADSET_UNPLUG = 1002;

    /**
     * Called when the user wants to resume the playback.
     * @param actionSource The source the listener got the action from.
     */
    void onPlay(int actionSource);

    /**
     * Called when the user wants to pause the playback.
     * @param actionSource The source the listener got the action from.
     */
    void onPause(int actionSource);

    /**
     * Called when the user wants to stop the playback.
     * @param actionSource The source the listener got the action from.
     */
    void onStop(int actionSource);

    /**
     * Called when the user performed one of the media actions (like fast forward or next track)
     * supported by MediaSession.
     * @param action The kind of the initated action.
     */
    void onMediaSessionAction(int action);

    /**
     * Called when the user performed a seek action through Media Session.
     * @param action The position to seek to in ms.
     */
    void onMediaSessionSeekTo(long pos);
}
