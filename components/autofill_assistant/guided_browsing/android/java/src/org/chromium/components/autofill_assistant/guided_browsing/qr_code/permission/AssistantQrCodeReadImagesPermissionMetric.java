// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import org.chromium.components.autofill_assistant.guided_browsing.GuidedBrowsingMetrics;
import org.chromium.components.autofill_assistant.guided_browsing.metrics.ReadImagesPermissionEvent;

/**
 * Read Images Permission Metric class containing all the read images permission metrics and method
 * to record read images permission metric.
 */
class AssistantQrCodeReadImagesPermissionMetric implements AssistantQrCodePermissionMetric {
    @Override
    public int getCheckingPermissionMetric() {
        return ReadImagesPermissionEvent.CHECKING_READ_IMAGES_PERMISSION;
    }

    @Override
    public int getAlreadyHadPermissionMetric() {
        return ReadImagesPermissionEvent.ALREADY_HAD_READ_IMAGES_PERMISSION;
    }

    @Override
    public int getCanPromptPermissionMetric() {
        return ReadImagesPermissionEvent.CAN_PROMPT_READ_IMAGES_PERMISSION;
    }

    @Override
    public int getCannotPromptPermissionMetric() {
        return ReadImagesPermissionEvent.CANNOT_PROMPT_READ_IMAGES_PERMISSION;
    }

    @Override
    public int getPermissionGrantedViaPromptMetric() {
        return ReadImagesPermissionEvent.READ_IMAGES_PERMISSION_GRANTED_VIA_PROMPT;
    }

    @Override
    public int getPermissionGrantedViaSettingsMetric() {
        return ReadImagesPermissionEvent.READ_IMAGES_PERMISSION_GRANTED_VIA_SETTINGS;
    }

    @Override
    public void recordPermissionMetric(int metric) {
        GuidedBrowsingMetrics.recordReadImagesPermissionMetric(metric);
    }
}
