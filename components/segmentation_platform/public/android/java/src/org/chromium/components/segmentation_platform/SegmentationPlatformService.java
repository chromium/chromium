// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.Callback;

/**
 * SegmentationPlatformService is the core class for segmentation platform.
 * It represents a native SegmentationPlatformService object in Java.
 */
public interface SegmentationPlatformService {
    /**
     * Called to get the segment selection result from the backend.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages. Currently unused.
     * @param callback The callback that contains the result of segmentation.
     */
    void getSelectedSegment(String segmentationKey, Callback<SegmentSelectionResult> callback);
}