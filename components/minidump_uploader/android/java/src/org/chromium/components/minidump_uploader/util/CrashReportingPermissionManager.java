// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader.util;

/** Interface for crash reporting permissions. */
public interface CrashReportingPermissionManager {
    /**
     * Checks whether this client is in-sample for crash reporting. See {@link
     * org.chromium.chrome.browser.metrics.UmaUtils#isClientInSampleForCrashes} for details.
     *
     * @return boolean Whether client is in-sample for crash reporting.
     */
    boolean isClientInSampleForCrashes();

    /**
     * Checks whether uploading of crash dumps is permitted for the available network(s).
     *
     * @return whether uploading crash dumps is permitted.
     */
    boolean isNetworkAvailableForCrashUploads();

    /**
     * Checks whether uploading of usage metrics and crash dumps is currently permitted, based on
     * user consent and policy only. This doesn't take network conditions or experimental state
     * (i.e. disabling upload) into consideration. A crash dump may be retried if this check passes.
     *
     * @return Whether usage and crash reporting is permitted.
     */
    default boolean isUsageAndCrashReportingPermitted() {
        return isUsageAndCrashReportingPermittedByUser()
                && isUsageAndCrashReportingPermittedByPolicy();
    }

    /**
     * Whether uploading of usage metrics and crash dumps is permitted by policy.
     * Important. Use {@link #isUsageAndCrashReportingPermitted} when checking if this device is
     * allowed to upload usage metrics and crash dumps.
     *
     * @return Whether usage and crash reporting is permitted by policy.
     */
    boolean isUsageAndCrashReportingPermittedByPolicy();

    /**
     * Whether uploading of usage metrics and crash dumps is permitted by user.
     * Important. Use {@link #isUsageAndCrashReportingPermitted} when checking if this device is
     * allowed to upload usage metrics and crash dumps.
     *
     * @return Whether usage and crash reporting is permitted by user.
     */
    boolean isUsageAndCrashReportingPermittedByUser();

    /**
     * Checks whether to ignore all consent and upload limitations for usage metrics and crash
     * reporting. Used by test devices to avoid a UI dependency.
     *
     * @return whether crash dumps should be uploaded if at all possible.
     */
    boolean isUploadEnabledForTests();
}
