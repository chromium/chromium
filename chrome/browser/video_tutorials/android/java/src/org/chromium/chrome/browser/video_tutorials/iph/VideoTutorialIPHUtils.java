// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Handles various feature utility functions associated with video tutorials IPH.
 */
public class VideoTutorialIPHUtils {
    /** @return The feature name to be used in IPH backend for the given {@code featureType}. */
    public static String getFeatureName(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DOWNLOAD:
                return FeatureConstants.VIDEO_TUTORIAL_DOWNLOAD_FEATURE;
            case FeatureType.SEARCH:
                return FeatureConstants.VIDEO_TUTORIAL_SEARCH_FEATURE;
            default:
                assert false;
                return null;
        }
    }

    /**
     * @return The event used in IPH backend when the IPH associated with the {@code featureType}
     *         is clicked.
     */
    public static String getClickEvent(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DOWNLOAD:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_DOWNLOAD;
            case FeatureType.SEARCH:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_SEARCH;
            default:
                assert false;
                return null;
        }
    }

    /**
     * @return The event used in IPH backend when the IPH associated with the {@code featureType}
     *         is dismissed.
     */
    public static String getDismissEvent(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DOWNLOAD:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_DOWNLOAD;
            case FeatureType.SEARCH:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_SEARCH;
            default:
                assert false;
                return null;
        }
    }

    /** @return The text used to show the video length on the IPH card. */
    public static String getVideoLengthString(int videoLength) {
        int minutes = videoLength / 60;
        int seconds = videoLength % 60;

        StringBuilder builder = new StringBuilder();
        builder.append(minutes);
        builder.append(":");
        builder.append(seconds);
        return builder.toString();
    }
}
