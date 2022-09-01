// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import org.chromium.components.autofill_assistant.guided_browsing.GuidedBrowsingMetrics;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.CameraPermissionEvent;

/**
 * Camera Permission Metric class containing all the camera permission metrics and method to record
 * camera permission metric.
 */
class AssistantQrCodeCameraPermissionMetric implements AssistantQrCodePermissionMetric {
    @Override
    public int getCheckingPermissionMetric() {
        return CameraPermissionEvent.CHECKING_CAMERA_PERMISSION;
    }

    @Override
    public int getAlreadyHadPermissionMetric() {
        return CameraPermissionEvent.ALREADY_HAD_CAMERA_PERMISSION;
    }

    @Override
    public int getCanPromptPermissionMetric() {
        return CameraPermissionEvent.CAN_PROMPT_CAMERA_PERMISSION;
    }

    @Override
    public int getCannotPromptPermissionMetric() {
        return CameraPermissionEvent.CANNOT_PROMPT_CAMERA_PERMISSION;
    }

    @Override
    public int getPermissionGrantedViaPromptMetric() {
        return CameraPermissionEvent.CAMERA_PERMISSION_GRANTED_VIA_PROMPT;
    }

    @Override
    public int getPermissionGrantedViaSettingsMetric() {
        return CameraPermissionEvent.CAMERA_PERMISSION_GRANTED_VIA_SETTINGS;
    }

    @Override
    public void recordPermissionMetric(int metric) {
        GuidedBrowsingMetrics.recordCameraPermissionMetric(metric);
    }
}
