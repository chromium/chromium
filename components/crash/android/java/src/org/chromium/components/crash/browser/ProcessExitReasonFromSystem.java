// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Wrapper to get the process exit reason of a dead process with same UID as current from
 * ActivityManager, and record to UMA.
 */
public class ProcessExitReasonFromSystem {
    private static ActivityManager sActivityManager;

    /**
     * Get the exit reason of the most recent chrome process that died and had |pid| as the process
     * ID. Only available on R+ devices, returns -1 otherwise.
     * @return ApplicationExitInfo.Reason
     */
    @RequiresApi(Build.VERSION_CODES.R)
    public static int getExitReason(int pid) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return -1;
        }
        ActivityManager am =
                sActivityManager != null
                        ? sActivityManager
                        : (ActivityManager)
                                ContextUtils.getApplicationContext()
                                        .getSystemService(Context.ACTIVITY_SERVICE);
        // Set maxNum to 1 since we want the latest reason with the pid.
        List<ApplicationExitInfo> reasons =
                am.getHistoricalProcessExitReasons(/* package_name= */ null, pid, /* maxNum= */ 1);
        if (reasons.isEmpty() || reasons.get(0) == null || reasons.get(0).getPid() != pid) {
            return -1;
        }
        return reasons.get(0).getReason();
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ExitReason.REASON_ANR,
        ExitReason.REASON_CRASH,
        ExitReason.REASON_CRASH_NATIVE,
        ExitReason.REASON_DEPENDENCY_DIED,
        ExitReason.REASON_EXCESSIVE_RESOURCE_USAGE,
        ExitReason.REASON_EXIT_SELF,
        ExitReason.REASON_INITIALIZATION_FAILURE,
        ExitReason.REASON_LOW_MEMORY,
        ExitReason.REASON_OTHER,
        ExitReason.REASON_PERMISSION_CHANGE,
        ExitReason.REASON_SIGNALED,
        ExitReason.REASON_UNKNOWN,
        ExitReason.REASON_USER_REQUESTED,
        ExitReason.REASON_USER_STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExitReason {
        int REASON_ANR = 0;
        int REASON_CRASH = 1;
        int REASON_CRASH_NATIVE = 2;
        int REASON_DEPENDENCY_DIED = 3;
        int REASON_EXCESSIVE_RESOURCE_USAGE = 4;
        int REASON_EXIT_SELF = 5;
        int REASON_INITIALIZATION_FAILURE = 6;
        int REASON_LOW_MEMORY = 7;
        int REASON_OTHER = 8;
        int REASON_PERMISSION_CHANGE = 9;
        int REASON_SIGNALED = 10;
        int REASON_UNKNOWN = 11;
        int REASON_USER_REQUESTED = 12;
        int REASON_USER_STOPPED = 13;
        int NUM_ENTRIES = 14;
    }

    @CalledByNative
    private static void recordExitReasonToUma(int pid, String umaName) {
        recordAsEnumHistogram(umaName, getExitReason(pid));
    }

    /**
     * Records the given |systemReason| (given by #getExitReason) to UMA with the given |umaName|.
     * @see #getExitReason
     */
    public static void recordAsEnumHistogram(String umaName, int systemReason) {
        Integer exitReason = convertApplicationExitInfoToExitReason(systemReason);
        if (exitReason != null) {
            RecordHistogram.recordEnumeratedHistogram(umaName, exitReason, ExitReason.NUM_ENTRIES);
        }
    }

    public static @Nullable Integer convertApplicationExitInfoToExitReason(int systemReason) {
        @ExitReason Integer reason = null;
        switch (systemReason) {
            case ApplicationExitInfo.REASON_ANR:
                reason = ExitReason.REASON_ANR;
                break;
            case ApplicationExitInfo.REASON_CRASH:
                reason = ExitReason.REASON_CRASH;
                break;
            case ApplicationExitInfo.REASON_CRASH_NATIVE:
                reason = ExitReason.REASON_CRASH_NATIVE;
                break;
            case ApplicationExitInfo.REASON_DEPENDENCY_DIED:
                reason = ExitReason.REASON_DEPENDENCY_DIED;
                break;
            case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE:
                reason = ExitReason.REASON_EXCESSIVE_RESOURCE_USAGE;
                break;
            case ApplicationExitInfo.REASON_EXIT_SELF:
                reason = ExitReason.REASON_EXIT_SELF;
                break;
            case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE:
                reason = ExitReason.REASON_INITIALIZATION_FAILURE;
                break;
            case ApplicationExitInfo.REASON_LOW_MEMORY:
                reason = ExitReason.REASON_LOW_MEMORY;
                break;
            case ApplicationExitInfo.REASON_OTHER:
                reason = ExitReason.REASON_OTHER;
                break;
            case ApplicationExitInfo.REASON_PERMISSION_CHANGE:
                reason = ExitReason.REASON_PERMISSION_CHANGE;
                break;
            case ApplicationExitInfo.REASON_SIGNALED:
                reason = ExitReason.REASON_SIGNALED;
                break;
            case ApplicationExitInfo.REASON_UNKNOWN:
                reason = ExitReason.REASON_UNKNOWN;
                break;
            case ApplicationExitInfo.REASON_USER_REQUESTED:
                reason = ExitReason.REASON_USER_REQUESTED;
                break;
            case ApplicationExitInfo.REASON_USER_STOPPED:
                reason = ExitReason.REASON_USER_STOPPED;
                break;
            default:
                break;
        }

        return reason;
    }

    @VisibleForTesting
    public static void setActivityManagerForTest(ActivityManager am) {
        sActivityManager = am;
    }
}
