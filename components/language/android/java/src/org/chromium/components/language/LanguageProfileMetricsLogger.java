// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class to record metrics about the user's Language Profile (ULP). */
public class LanguageProfileMetricsLogger {
    @VisibleForTesting
    static final String INITIATION_STATUS_HISTOGRAM = "LanguageUsage.ULP.Initiation.Status";

    @VisibleForTesting
    static final String SIGNED_IN_INITIATION_STATUS_HISTOGRAM =
            "LanguageUsage.ULP.Initiation.Status.SignedIn";

    @VisibleForTesting
    static final String SIGNED_OUT_INITIATION_STATUS_HISTOGRAM =
            "LanguageUsage.ULP.Initiation.Status.DefaultAccount";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ULPInitiationStatus.SUCCESS,
        ULPInitiationStatus.NOT_SUPPORTED,
        ULPInitiationStatus.TIMED_OUT,
        ULPInitiationStatus.FAILURE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ULPInitiationStatus {
        int SUCCESS = 0;
        int NOT_SUPPORTED = 1;
        int TIMED_OUT = 2;
        int FAILURE = 3;

        // STOP: When updating this, also update values in enums.xml and make sure to update the
        // IntDef above.
        int NUM_ENTRIES = 4;
    }

    public static void recordInitiationStatus(
            boolean signedIn, @ULPInitiationStatus int initStatus) {
        RecordHistogram.recordEnumeratedHistogram(
                INITIATION_STATUS_HISTOGRAM, initStatus, ULPInitiationStatus.NUM_ENTRIES);
        if (signedIn) {
            RecordHistogram.recordEnumeratedHistogram(
                    SIGNED_IN_INITIATION_STATUS_HISTOGRAM,
                    initStatus,
                    ULPInitiationStatus.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    SIGNED_OUT_INITIATION_STATUS_HISTOGRAM,
                    initStatus,
                    ULPInitiationStatus.NUM_ENTRIES);
        }
    }
}
