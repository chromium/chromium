// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/**
 * Java side of the JNI bridge for various JNI conversions required by the segmentation platform.
 */
@JNINamespace("segmentation_platform")
public class SegmentationPlatformConversionBridge {
    @CalledByNative
    private static SegmentSelectionResult createSegmentSelectionResult(
            boolean isReady, int selectedSegment) {
        SegmentId segment = SegmentId.forNumber(selectedSegment);
        if (segment == null) segment = SegmentId.OPTIMIZATION_TARGET_UNKNOWN;
        return new SegmentSelectionResult(isReady, segment);
    }

    @CalledByNative
    private static OnDemandSegmentSelectionResult createOnDemandSegmentSelectionResult(
            SegmentSelectionResult selectedSegment, TriggerContext triggerContext) {
        return new OnDemandSegmentSelectionResult(selectedSegment, triggerContext);
    }
}
