// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import org.chromium.chrome.browser.video_tutorials.FeatureType;

/**
 * This class acts as a temporary tracker of the user click on Try Now button on the video player
 * during a chrome session. As soon as a
 */
public interface VideoTutorialTryNowTracker {
    /**
     * Called to record that the Try Now button has been clicked by the user, and we should show the
     * appropriate UI.
     * @param featureType The feature type associated with the tutorial.
     */
    void recordTryNowButtonClicked(@FeatureType int featureType);

    /**
     * Called to determine whether or not the Try Now button was clicked by the user.
     * @param featureType The feature type associated with the tutorial.
     * @return Whether or not the Try Now button was clicked for a video tutorial.
     */
    boolean didClickTryNowButton(@FeatureType int featureType);

    /**
     * Called when the try now UI was shown. Serves as a signal to reset the internal state of the
     * tracker.
     */
    void tryNowUIShown(@FeatureType int featureType);
}