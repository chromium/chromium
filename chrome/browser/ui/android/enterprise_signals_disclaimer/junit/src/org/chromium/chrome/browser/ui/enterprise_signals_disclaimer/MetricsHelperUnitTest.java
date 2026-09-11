// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.MetricsHelper.ShownOn;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link MetricsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MetricsHelperUnitTest {
    @Rule public final FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private MetricsHelper mMetricsHelper;

    @Before
    public void setUp() {
        mMetricsHelper = new MetricsHelper();
    }

    @Test
    public void testRecordShown_startup() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN, ShownOn.STARTUP);

        MetricsHelper.recordShown(ShownOn.STARTUP);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordShown_signIn() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        MetricsHelper.HISTOGRAM_SHOWN, ShownOn.SIGN_IN);

        MetricsHelper.recordShown(ShownOn.SIGN_IN);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordTimeToUserAction() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        MetricsHelper.recordTimeToUserAction(1234L);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_tappedAccept_withoutPriorDismissals() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 0)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_tappedSignOut_doesNotRecordImplicitDismissalsBeforeAcceptance() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_SIGN_OUT)
                        .expectNoRecords(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_SIGN_OUT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_implicitDismissals_countedOnAcceptance() {
        // Record implicit dismissals within the 5-minute threshold.
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_BACK_PRESS);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_SWIPE_DOWN);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_TAP_OUTSIDE);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 3)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_acceptanceClearsImplicitDismissals() {
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_BACK_PRESS);
        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        // Subsequent acceptance should record 0 implicit dismissals.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 0)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_expiredImplicitDismissals_notCountedOnAcceptance() {
        // First implicit dismissal happens now.
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_BACK_PRESS);

        // Advance time past the threshold (threshold + 1 ms).
        mFakeTimeTestRule.advanceMillis(MetricsHelper.IMPLICIT_DISMISSAL_THRESHOLD_MILLIS + 1);

        // Second implicit dismissal happens within the new window.
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_SWIPE_DOWN);

        // On accept, only the second dismissal should be counted.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 1)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordResult_nonImplicitDismissal_notCountedOnAcceptance() {
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION);
        mMetricsHelper.recordResult(DismissalCause.DISMISSED_BY_CLOSE_BUTTON);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 0)
                        .build();

        mMetricsHelper.recordResult(DismissalCause.TAPPED_ACCEPT);

        histogramWatcher.assertExpected();
    }
}
