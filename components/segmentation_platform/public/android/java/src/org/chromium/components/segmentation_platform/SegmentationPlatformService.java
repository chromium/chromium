// Copyright 2021 The Chromium Authors
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
     * Called to get the segment selection result asynchronously from the backend.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages.
     * @param callback The callback that contains the result of segmentation.
     */
    void getSelectedSegment(String segmentationKey, Callback<SegmentSelectionResult> callback);

    /**
     * Gets a classification result asynchronously from segmentation. It's the java counterpart to
     * GetClassificationResult in segmentation_platform_service.h.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages.
     * @param predictionOptions Options to get the classification result (e.g. whether to get a
     *         cached result or run the model).
     * @param inputContext (Optional) Instance of InputContext with input signals to model.
     * @param callback Callback containing the classification result.
     */
    void getClassificationResult(
            String segmentationKey,
            PredictionOptions predictionOptions,
            InputContext inputContext,
            Callback<ClassificationResult> callback);

    /**
     * Called to get the segment selection result synchronously from the backend.
     * @deprecated in favor of {@link getSelectedSegment}.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages.
     * @return The result of segment selection
     */
    @Deprecated
    SegmentSelectionResult getCachedSegmentResult(String segmentationKey);
}
