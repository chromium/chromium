// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

/**
 * Generic interface used to control the playback of media content.
 * Changes to the media content status will be propagated via the MediaStatusObserver interface.
 */
public interface MediaController {
    /**
     * Start playing the media if it is paused. Is a no-op if not supported by the media or the
     * media is already playing.
     */
    public void play();

    /**
     * Pauses the media if it is playing. Is a no-op if not supported by the media or the media is
     * already paused.
     */
    public void pause();

    /**
     * Mutes the media if |mute| is true, and unmutes it if false. Is a no-op if not supported by
     * the media.
     */
    public void setMute(boolean mute);

    /**
     * Changes the current volume of the media, with 1 being the highest and 0 being the lowest/no
     * sound. Does not change the (un)muted state of the media. Is a no-op if not supported by the
     * media.
     */
    public void setVolume(double volume);

    /**
     * Sets the current playback position. |position| must be less than or equal to the duration of
     * the media. Is a no-op if the media doesn't support seeking.
     */
    public void seek(long position);
}
