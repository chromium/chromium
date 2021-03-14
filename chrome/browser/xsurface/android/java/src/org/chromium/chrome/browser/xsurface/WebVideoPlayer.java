// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.ViewGroup;

/** Interface for controlling video playing via web. */
public interface WebVideoPlayer {
    /** Interface for listening to the video player events. */
    public interface Listener {
        /** Called when the player state gets changed. */
        default void onStateChanged(int newState) {}

        /** Called when an error occurs. */
        default void onError(int errorCode) {}
    }

    /** Destroys this video player instance. */
    default void destroy() {}

    /** Prepares the video for playing. */
    default void prepare() {}

    /** Starts playing the video. */
    default void play(ViewGroup container) {}

    /** Pauses the video. */
    default void pause() {}

    /** Stops the video. */
    default void stop() {}

    /** Returns the elapsed time in seconds since the video started playing. */
    default long getElapsedTimeInSeconds() {
        return 0;
    }

    /** Adds a listener to be notified for the video playing events. */
    default void addListener(Listener listener) {}

    /** Removes the listener so it's not longer notified of player events. */
    default void removeListener(Listener listener) {}
}
