// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import java.util.List;
import java.util.function.Function;

/** Violation that occurred. */
public class Violation {
    public static final int DETECT_FAILED = 0x00;
    // Taken from android.os.StrictMode
    public static final int DETECT_DISK_WRITE = 0x01;
    public static final int DETECT_DISK_READ = 0x02;
    // In almost all cases that writes needs to be masked, so do reads. Simplify that case.
    public static final int DETECT_DISK_IO = DETECT_DISK_WRITE | DETECT_DISK_READ;

    // Package since apps should always fix this.
    static final int DETECT_RESOURCE_MISMATCH = 0x10;

    // Bitmask with all of the thread strict mode violations that we are interested in.
    public static final int DETECT_ALL_KNOWN = DETECT_DISK_IO | DETECT_RESOURCE_MISMATCH;

    private final int mViolationType;
    private final StackTraceElement[] mStackTrace;

    public Violation(int violationType, StackTraceElement[] stackTrace) {
        mViolationType = violationType;
        mStackTrace = stackTrace;
    }

    boolean isInWhitelist(List<Function<Violation, Integer>> whitelist) {
        for (Function<Violation, Integer> whitelistEntry : whitelist) {
            Integer mask = whitelistEntry.apply(this);
            if (mask != null && (mViolationType & mask) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * Type of violation occurred, bitmask containing 0 or more of the DETECT_* constants from AOSP.
     *
     * Returns DETECT_ALL_KNOWN if the violation type should be ignored.
     */
    public int violationType() {
        return mViolationType;
    }

    public StackTraceElement[] stackTrace() {
        return mStackTrace;
    }

    /**
     * A human readable string describing the type of violation that occurred. If multiple
     * violations occurred, this will only contain one of them.
     */
    public final String violationString() {
        int type = violationType();
        if ((type & DETECT_DISK_WRITE) != 0) {
            return "DISK_WRITE";
        } else if ((type & DETECT_DISK_READ) != 0) {
            return "DISK_READ";
        } else if ((type & DETECT_RESOURCE_MISMATCH) != 0) {
            return "RESOURCE_MISMATCH";
        }
        return "UNKNOWN";
    }
}
