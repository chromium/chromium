// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.media_session.MediaPosition;

/**
 * Responsible for observing a media session and notifying the observer about play/pause/end media
 * events.
 */
public class PlaybackStateObserver extends MediaSessionObserver {
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
    }

    private final Observer mObserver;
    private MediaPosition mMediaPosition;

    /** Constructor. */
    public PlaybackStateObserver(WebContents webContents, Observer observer) {
        super(MediaSession.fromWebContents(webContents));
        mObserver = observer;
    }

    @Override
    public void mediaSessionPositionChanged(MediaPosition position) {
        if (position == null) return;
        mMediaPosition = position;
    }

    @Override
    public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        boolean playerEnded = !isControllable && isSuspended && mMediaPosition != null
                && mMediaPosition.getPosition() > 0.5 * mMediaPosition.getDuration();
        boolean playerPaused = isControllable && isSuspended;
        boolean isPlaying = isControllable && !isSuspended;
        // TODO(shaktisahu): Fix these signals and logic in another CL.
        if (isPlaying) {
            mObserver.onPlay();
        }
        if (playerPaused) {
            mObserver.onPause();
        }
        if (playerEnded) {
            mObserver.onEnded();
        }
    }
}
