// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import org.chromium.chrome.browser.video_tutorials.FeatureType;

/**
 * {@link VideoTutorialTryNowTracker} implementation.
 */
public class TryNowTrackerImpl implements VideoTutorialTryNowTracker {
    private @FeatureType int mFeatureType = FeatureType.INVALID;

    @Override
    public void recordTryNowButtonClicked(@FeatureType int featureType) {
        mFeatureType = featureType;
    }

    @Override
    public boolean didClickTryNowButton(@FeatureType int featureType) {
        return mFeatureType == featureType;
    }

    @Override
    public void tryNowUIShown(@FeatureType int featureType) {
        mFeatureType = FeatureType.INVALID;
    }
}