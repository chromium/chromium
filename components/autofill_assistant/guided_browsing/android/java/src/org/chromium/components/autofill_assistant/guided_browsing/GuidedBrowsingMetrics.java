// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.CameraPermissionEvent;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.ParseSingleTagXmlActionEvent;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.ReadImagesPermissionEvent;

/**
 * Records user actions and histograms related to |guided_browsing| package.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
public class GuidedBrowsingMetrics {
    /**
     * Records the actions performed to grant the camera permission.
     */
    public static void recordCameraPermissionMetric(@CameraPermissionEvent int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.Camera.PermissionEvent", metric,
                CameraPermissionEvent.MAX_VALUE + 1);
    }

    /**
     * Records the actions performed to grant the read images permission.
     */
    public static void recordReadImagesPermissionMetric(@ReadImagesPermissionEvent int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.ReadImages.PermissionEvent", metric,
                ReadImagesPermissionEvent.MAX_VALUE + 1);
    }

    /**
     * Records the events performed during ParseSingleTagXml action.
     */
    public static void recordParseSingleTagXmlActionEvent(
            @ParseSingleTagXmlActionEvent int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.ParseSingleTagXml.ActionEvent", metric,
                ParseSingleTagXmlActionEvent.MAX_VALUE + 1);
    }
}
