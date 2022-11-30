// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import org.chromium.chrome.browser.video_tutorials.Tutorial;

/**
 * Creates the player URL for a video tutorial.
 */
class VideoPlayerURLBuilder {
    // TODO(shaktisahu): Move this to UrlConstants.
    private static final String VIDEO_PLAYER_URL = "chrome-untrusted://video-tutorials/";

    /** Constructs the player URL for a given video tutorial. */
    public static String buildFromTutorial(Tutorial tutorial) {
        StringBuilder builder = new StringBuilder();
        builder.append(VIDEO_PLAYER_URL);
        builder.append("?");
        builder.append("video_url=");
        builder.append(tutorial.videoUrl);
        builder.append("&poster_url=");
        builder.append(tutorial.posterUrl);
        builder.append("&caption_url=");
        builder.append(tutorial.captionUrl);
        return builder.toString();
    }
}
