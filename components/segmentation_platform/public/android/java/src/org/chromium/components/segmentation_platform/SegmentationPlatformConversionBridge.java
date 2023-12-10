// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/** Provides JNI conversion methods for public data types provided by segmentation platform. */
@JNINamespace("segmentation_platform")
public class SegmentationPlatformConversionBridge {
    @CalledByNative
    private static SegmentSelectionResult createSegmentSelectionResult(
            boolean isReady, int selectedSegment, boolean hasRank, float rank) {
        SegmentId segment = SegmentId.forNumber(selectedSegment);
        if (segment == null) segment = SegmentId.OPTIMIZATION_TARGET_UNKNOWN;
        Float optionalRank = hasRank ? rank : null;
        return new SegmentSelectionResult(isReady, segment, optionalRank);
    }

    @CalledByNative
    private static ClassificationResult createClassificationResult(
            int status, String[] orderedLabels) {
        return new ClassificationResult(status, orderedLabels);
    }
}
