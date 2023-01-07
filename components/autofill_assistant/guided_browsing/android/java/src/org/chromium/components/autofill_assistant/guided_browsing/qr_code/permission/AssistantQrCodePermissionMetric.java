// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

/** Permission metrics interface containing all the metrics and method to log the metric. */
interface AssistantQrCodePermissionMetric {
    /** The metric for CHECKING_PERMISSION. */
    public int getCheckingPermissionMetric();

    /** The metric for ALREADY_HAD_PERMISSION. */
    public int getAlreadyHadPermissionMetric();

    /** The metric for CAN_PROMPT_PERMISSION. */
    public int getCanPromptPermissionMetric();

    /** The metric for CANNOT_PROMPT_PERMISSION. */
    public int getCannotPromptPermissionMetric();

    /** The metric for PERMISSION_GRANTED_VIA_PROMPT. */
    public int getPermissionGrantedViaPromptMetric();

    /** The metric for PERMISSION_GRANTED_VIA_SETTINGS. */
    public int getPermissionGrantedViaSettingsMetric();

    /** Logs the metric. */
    public void recordPermissionMetric(int metric);
}
