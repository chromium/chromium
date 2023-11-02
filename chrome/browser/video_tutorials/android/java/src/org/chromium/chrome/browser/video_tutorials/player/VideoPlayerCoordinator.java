// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.view.View;

import org.chromium.chrome.browser.video_tutorials.Tutorial;

/**
 * The top level coordinator for the video player.
 */
public interface VideoPlayerCoordinator {
    /**
     * Entry point for playing a video tutorial.
     */
    void playVideoTutorial(Tutorial tutorial);

    /** @return The Android {@link View} representing this widget. */
    View getView();

    /** To be called when the back button is pressed. */
    boolean onBackPressed();

    /** Tears down this coordinator. */
    void destroy();
}
