// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

/**
 * Class encapsulating data needed to show a video tutorial on the UI.
 */
public class Tutorial {
    public final @FeatureType int featureType;
    public final String title;
    public final String accessibilityText;
    public final String videoUrl;
    public final String posterUrl;
    public final String animatedGifUrl;
    public final String thumbnailUrl;
    public final String captionUrl;
    public final String shareUrl;
    public final int videoLength;

    /** Constructor */
    public Tutorial(@FeatureType int featureType, String title, String videoUrl, String posterUrl,
            String animatedGifUrl, String thumbnailUrl, String captionUrl, String shareUrl,
            int videoLength) {
        this.featureType = featureType;
        this.title = title;
        this.accessibilityText = title;
        this.videoUrl = videoUrl;
        this.posterUrl = posterUrl;
        this.animatedGifUrl = animatedGifUrl;
        this.thumbnailUrl = thumbnailUrl;
        this.captionUrl = captionUrl;
        this.shareUrl = shareUrl;
        this.videoLength = videoLength;
    }
}
