// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.bridges;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.video_tutorials.Tutorial;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class to provide conversion methods between C++ and Java for video tutorials.
 */
@JNINamespace("video_tutorials")
public class TutorialConversionBridge {
    @CalledByNative
    private static List<Tutorial> createTutorialList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static Tutorial createTutorialAndMaybeAddToList(@Nullable List<Tutorial> list,
            int featureType, String title, String videoUrl, String posterUrl, String animatedGifUrl,
            String thumbnailUrl, String captionUrl, String shareUrl, int videoLength) {
        Tutorial tutorial = new Tutorial(featureType, title, videoUrl, posterUrl, animatedGifUrl,
                thumbnailUrl, captionUrl, shareUrl, videoLength);
        if (list != null) list.add(tutorial);
        return tutorial;
    }
}
