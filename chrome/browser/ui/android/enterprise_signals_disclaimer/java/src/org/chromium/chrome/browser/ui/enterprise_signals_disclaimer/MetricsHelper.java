// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.concurrent.TimeUnit;

/** This class is stateful and must persist across multiple disclaimer instances. */
@NullMarked
class MetricsHelper {
    @VisibleForTesting
    static final String HISTOGRAM_SHOWN = "Enterprise.SignalsDisclaimer.ExistingProfiles.Shown";

    @VisibleForTesting
    static final String HISTOGRAM_RESULT = "Enterprise.SignalsDisclaimer.ExistingProfiles.Result";

    @VisibleForTesting
    static final String HISTOGRAM_TIME_TO_USER_ACTION =
            "Enterprise.SignalsDisclaimer.ExistingProfiles.TimeToUserAction";

    @VisibleForTesting
    static final String HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE =
            "Enterprise.SignalsDisclaimer.ExistingProfiles.ImplicitDismissalsBeforeAcceptance";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(EnterpriseSignalsDisclaimerShownOn)
    @IntDef({ShownOn.STARTUP, ShownOn.SIGN_IN, ShownOn.COUNT})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShownOn {
        int STARTUP = 0;
        int SIGN_IN = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSignalsDisclaimerShownOn)

    public static void recordShown(@ShownOn int shownOn) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_SHOWN, shownOn, ShownOn.COUNT);
    }

    public static void recordTimeToUserAction(long timeToUserActionMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_TIME_TO_USER_ACTION, timeToUserActionMillis);
    }

    private static boolean resultIsImplicitDismissal(@DismissalCause int result) {
        return result == DismissalCause.DISMISSED_BY_BACK_PRESS
                || result == DismissalCause.DISMISSED_BY_SWIPE_DOWN
                || result == DismissalCause.DISMISSED_BY_TAP_OUTSIDE;
    }

    @VisibleForTesting
    static final long IMPLICIT_DISMISSAL_THRESHOLD_MILLIS = TimeUnit.MINUTES.toMillis(5);

    private final Deque<Long> mImplicitDismissalTimestamps = new ArrayDeque<>();

    private void removeOldImplicitDismissals() {
        final long now = TimeUtils.elapsedRealtimeMillis();
        final long cutoff = now - IMPLICIT_DISMISSAL_THRESHOLD_MILLIS;
        while (!mImplicitDismissalTimestamps.isEmpty()
                && mImplicitDismissalTimestamps.peekFirst() < cutoff) {
            mImplicitDismissalTimestamps.pollFirst();
        }
    }

    private static final int MAX_IMPLICIT_DISMISSALS = 10;

    /**
     * This function mainly records the result of the disclaimer interaction. However, it also keeps
     * track of implicit dismissals, so that on acceptance we can record how many times the user
     * dismissed the dialog implicitly in the last `IMPLICIT_DISMISSAL_THRESHOLD_MILLIS`
     * milliseconds.
     */
    public void recordResult(@DismissalCause int result) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_RESULT, result, DismissalCause.COUNT);

        removeOldImplicitDismissals();

        // If the user accepted the disclaimer, flush the timestamp queue to the histogram.
        if (result == DismissalCause.TAPPED_ACCEPT) {
            // We are only interested in the number of implicit dismissals before the user accepts,
            // this is meant to gauge if the users are confused by the silent signout on implicit
            // dismissal.
            RecordHistogram.recordExactLinearHistogram(
                    HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE,
                    mImplicitDismissalTimestamps.size(),
                    MAX_IMPLICIT_DISMISSALS);
            mImplicitDismissalTimestamps.clear();
        } else if (resultIsImplicitDismissal(result)) {
            mImplicitDismissalTimestamps.add(TimeUtils.elapsedRealtimeMillis());
        }
    }
}
