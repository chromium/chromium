// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.CreateCredentialNoCreateOptionException;
import androidx.credentials.exceptions.CreateCredentialUnknownException;
import androidx.credentials.exceptions.GetCredentialCancellationException;
import androidx.credentials.exceptions.GetCredentialCustomException;
import androidx.credentials.exceptions.GetCredentialException;
import androidx.credentials.exceptions.GetCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialProviderConfigurationException;
import androidx.credentials.exceptions.GetCredentialUnknownException;
import androidx.credentials.exceptions.GetCredentialUnsupportedException;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests for the ThirdPartyCredentialManagerMetricsRecorder. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class ThirdPartyCredentialManagerMetricsRecorderTest {

    private static class FakeCreateCredentialException extends CreateCredentialException {
        public FakeCreateCredentialException() {
            super("FAKE_CREATE", "Fake create");
        }
    }

    private static class FakeGetCredentialException extends GetCredentialException {
        public FakeGetCredentialException() {
            super("FAKE_GET", "Fake get");
        }
    }

    @Test
    public void testRecordingCredentialManagerStoreResultSucceeds() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.SUCCESS);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(true, null);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerStoreResultFailsWhenUserCancels() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.USER_CANCELED);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                false, new CreateCredentialCancellationException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerStoreResultFailsWithoutCreateOptions() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.NO_CREATE_OPTIONS);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                false, new CreateCredentialNoCreateOptionException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerStoreResultFailsWhenInterrupted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.INTERRUPTED);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                false, new CreateCredentialInterruptedException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerStoreResultFailsForUnknownReason() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.UNKNOWN);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                false, new CreateCredentialUnknownException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerStoreResultFailsForUnexpectedError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        CredentialManagerStoreResult.UNEXPECTED_ERROR);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                false, new FakeCreateCredentialException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultSucceeds() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.SUCCESS);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(true, null);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsWhenUserCancels() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.USER_CANCELED);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialCancellationException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultSFailsWithCustomError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.CUSTOM_ERROR);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialCustomException("CUSTOM_TYPE", "Custom type"));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsWhenInterrupted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.INTERRUPTED);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialInterruptedException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsBecauseOfProviderConfig() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.PROVIDER_CONFIGURATION_ERROR);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialProviderConfigurationException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsWithUnknownError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.UNKNOWN);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialUnknownException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsWithUnsupportedError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.UNSUPPORTED);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new GetCredentialUnsupportedException());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordingCredentialManagerGetResultFailsWithUnexpectedError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.GET_RESULT_HISTOGRAM_NAME,
                        CredentialManagerAndroidGetResult.UNEXPECTED_ERROR);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerGetResult(
                false, new FakeGetCredentialException());
        histogramWatcher.assertExpected();
    }
}
