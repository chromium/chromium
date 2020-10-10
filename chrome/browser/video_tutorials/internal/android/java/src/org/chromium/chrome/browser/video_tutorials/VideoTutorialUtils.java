// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.base.Callback;

import java.util.List;
import java.util.Locale;

/**
 * Handles various feature utility functions associated with video tutorials UI.
 */
public class VideoTutorialUtils {
    /**
     * Converts a duration string in ms to a human-readable form.
     * @param videoLengthSeconds The video length in seconds.
     * @return The video length in human-readable form.
     */
    public static String getVideoLengthString(int videoLengthSeconds) {
        int hours = videoLengthSeconds / 3600;
        int minutes = (videoLengthSeconds / 60) % 60;
        int seconds = videoLengthSeconds % 60;

        if (hours > 0) {
            return String.format(Locale.US, "%d:%02d:%02d", hours, minutes, seconds);
        } else {
            return String.format(Locale.US, "%d:%02d", minutes, seconds);
        }
    }

    /**
     * Finds the next video tutorial to be presented to the user after the user has completed one.
     */
    public static void getNextTutorial(VideoTutorialService videoTutorialService, Tutorial tutorial,
            Callback<Tutorial> callback) {
        videoTutorialService.getTutorials(tutorials -> {
            Tutorial nextTutorial = VideoTutorialUtils.getNextTutorial(tutorials, tutorial);
            callback.onResult(nextTutorial);
        });
    }

    /** @return Whether or not to show the Try Now button on the video player. */
    public static boolean shouldShowTryNow(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DOWNLOAD:
            case FeatureType.SEARCH:
            case FeatureType.VOICE_SEARCH:
                return true;
            case FeatureType.CHROME_INTRO:
            default:
                return false;
        }
    }

    private static Tutorial getNextTutorial(List<Tutorial> tutorials, Tutorial currentTutorial) {
        int currentIndex = 0;
        for (int i = 0; i < tutorials.size(); i++) {
            if (tutorials.get(i).featureType == currentTutorial.featureType) {
                currentIndex = i;
                break;
            }
        }

        return currentIndex < tutorials.size() - 1 ? tutorials.get(currentIndex + 1) : null;
    }
}
