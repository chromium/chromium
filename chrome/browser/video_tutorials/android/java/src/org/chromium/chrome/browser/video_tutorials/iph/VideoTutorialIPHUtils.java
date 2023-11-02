// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Handles various feature utility functions associated with video tutorials IPH.
 */
public class VideoTutorialIPHUtils {
    /**
     * @return The feature name to be used in IPH backend for the given {@code featureType} in order
     *         to show the feature IPH on NTP.
     */
    public static @Nullable String getFeatureNameForNTP(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.SUMMARY:
                return FeatureConstants.VIDEO_TUTORIAL_NTP_SUMMARY_FEATURE;
            case FeatureType.CHROME_INTRO:
                return FeatureConstants.VIDEO_TUTORIAL_NTP_CHROME_INTRO_FEATURE;
            case FeatureType.DOWNLOAD:
                return FeatureConstants.VIDEO_TUTORIAL_NTP_DOWNLOAD_FEATURE;
            case FeatureType.SEARCH:
                return FeatureConstants.VIDEO_TUTORIAL_NTP_SEARCH_FEATURE;
            case FeatureType.VOICE_SEARCH:
                return FeatureConstants.VIDEO_TUTORIAL_NTP_VOICE_SEARCH_FEATURE;
            default:
                // It's possible that there are more feature types known to server than the client.
                // Don't show an IPH for those tutorials.
                return null;
        }
    }

    /**
     * @return The event used in IPH backend when the IPH associated with the {@code featureType}
     *         on NTP is clicked.
     */
    public static String getClickEvent(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.SUMMARY:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_SUMMARY;
            case FeatureType.CHROME_INTRO:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_CHROME_INTRO;
            case FeatureType.DOWNLOAD:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_DOWNLOAD;
            case FeatureType.SEARCH:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_SEARCH;
            case FeatureType.VOICE_SEARCH:
                return EventConstants.VIDEO_TUTORIAL_CLICKED_VOICE_SEARCH;
            default:
                assert false;
                return null;
        }
    }

    /**
     * @return The event used in IPH backend when the IPH associated with the {@code featureType}
     *         on NTP is dismissed.
     */
    public static String getDismissEvent(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.SUMMARY:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_SUMMARY;
            case FeatureType.CHROME_INTRO:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_CHROME_INTRO;
            case FeatureType.DOWNLOAD:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_DOWNLOAD;
            case FeatureType.SEARCH:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_SEARCH;
            case FeatureType.VOICE_SEARCH:
                return EventConstants.VIDEO_TUTORIAL_DISMISSED_VOICE_SEARCH;
            default:
                assert false;
                return null;
        }
    }

}
