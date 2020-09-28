// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Handles various feature utility functions associated with video tutorials UI.
 */
public class VideoTutorialUtils {
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

    private static Tutorial getNextTutorial(List<Tutorial> tutorials, Tutorial currentTutorial) {
        int currentTutorialIndex = 0;
        for (int i = 0; i < tutorials.size(); i++) {
            if (tutorials.get(i).featureType == currentTutorial.featureType) break;
        }

        return currentTutorialIndex < tutorials.size() - 1 ? tutorials.get(currentTutorialIndex + 1)
                                                           : null;
    }
}
