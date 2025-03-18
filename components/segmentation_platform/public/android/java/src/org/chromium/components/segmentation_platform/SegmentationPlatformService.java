// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

import java.util.Set;

/**
 * SegmentationPlatformService is the core class for segmentation platform. It represents a native
 * SegmentationPlatformService object in Java.
 */
@NullMarked
public interface SegmentationPlatformService {
    /**
     * Called to get the segment selection result asynchronously from the backend.
     *
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *     usages.
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
     *
     * @deprecated in favor of {@link getSelectedSegment}.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *     usages.
     * @return The result of segment selection
     */
    @Deprecated
    SegmentSelectionResult getCachedSegmentResult(String segmentationKey);

    /**
     * Gets the list of input keys needed for a segmentation key.
     *
     * @param segmentationKey The key to fetch keys for.
     * @param callback Callback with the list of input keys.
     */
    void getInputKeysForModel(String segmentationKey, Callback<Set<String>> callback);

    /**
     * Called to trigger training data collection.
     *
     * @param segmentId Id associated with the segment to collect training data for.
     * @param requestId Id associated with a |getClassificationResult| call.
     * @param ukmSourceId Used to attach the training data to the right URL.
     * @param param Used to pass one additional output feature to be uploaded as training data. It
     *     is recommended that the additional feature is also recorded as UMA histogram.
     * @param callback Callback containing the status of the training data collection.
     */
    void collectTrainingData(
            int segmentId,
            long requestId,
            long ukmSourceId,
            TrainingLabels param,
            Callback<Boolean> callback);
}
