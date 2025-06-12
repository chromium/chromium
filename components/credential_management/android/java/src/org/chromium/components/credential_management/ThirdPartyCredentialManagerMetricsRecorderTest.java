// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.CreateCredentialNoCreateOptionException;
import androidx.credentials.exceptions.CreateCredentialUnknownException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.Arrays;
import java.util.Collection;

/** Tests for the ThirdPartyCredentialManagerMetricsRecorder. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class ThirdPartyCredentialManagerMetricsRecorderTest {

    private static class FakeCreateCredentialException extends CreateCredentialException {
        public FakeCreateCredentialException() {
            super("FAKE_TYPE", "Fake type");
        }
    }

    @Parameters
    public static Collection testCases() {
        return Arrays.asList(
                new Object[][] {
                    {true, null, CredentialManagerStoreResult.SUCCESS},
                    {
                        false,
                        new CreateCredentialCancellationException(),
                        CredentialManagerStoreResult.USER_CANCELED
                    },
                    {
                        false,
                        new CreateCredentialNoCreateOptionException(),
                        CredentialManagerStoreResult.NO_CREATE_OPTIONS
                    },
                    {
                        false,
                        new CreateCredentialInterruptedException(),
                        CredentialManagerStoreResult.INTERRUPTED
                    },
                    {
                        false,
                        new CreateCredentialUnknownException(),
                        CredentialManagerStoreResult.UNKNOWN
                    },
                    {
                        false,
                        new FakeCreateCredentialException(),
                        CredentialManagerStoreResult.UNEXPECTED_ERROR
                    }
                });
    }

    private final boolean mSuccess;
    private final CreateCredentialException mError;
    private final int mResult;

    public ThirdPartyCredentialManagerMetricsRecorderTest(
            boolean success, CreateCredentialException error, int result) {
        mSuccess = success;
        mError = error;
        mResult = result;
    }

    @Test
    public void testRecordingCredentialManagerStoreResult() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ThirdPartyCredentialManagerMetricsRecorder.STORE_RESULT_HISTOGRAM_NAME,
                        mResult);
        ThirdPartyCredentialManagerMetricsRecorder.recordCredentialManagerStoreResult(
                mSuccess, mError);
        histogramWatcher.assertExpected();
    }
}
