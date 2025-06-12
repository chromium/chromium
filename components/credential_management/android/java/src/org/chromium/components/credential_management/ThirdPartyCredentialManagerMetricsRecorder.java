// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.credential_management;

import androidx.annotation.Nullable;
import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.CreateCredentialNoCreateOptionException;
import androidx.credentials.exceptions.CreateCredentialUnknownException;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/** Helper class for recording third party credential manager metrics. */
@NullMarked
public final class ThirdPartyCredentialManagerMetricsRecorder {
    public static final String STORE_RESULT_HISTOGRAM_NAME =
            "PasswordManager.CredentialRequest.ThirdParty.Store";

    private ThirdPartyCredentialManagerMetricsRecorder() {}

    public static void recordCredentialManagerStoreResult(
            boolean success, @Nullable CreateCredentialException error) {
        int result = CredentialManagerStoreResult.SUCCESS;
        if (!success) {
            if (error instanceof CreateCredentialCancellationException) {
                result = CredentialManagerStoreResult.USER_CANCELED;
            } else if (error instanceof CreateCredentialNoCreateOptionException) {
                result = CredentialManagerStoreResult.NO_CREATE_OPTIONS;
            } else if (error instanceof CreateCredentialInterruptedException) {
                result = CredentialManagerStoreResult.INTERRUPTED;
            } else if (error instanceof CreateCredentialUnknownException) {
                result = CredentialManagerStoreResult.UNKNOWN;
            } else {
                result = CredentialManagerStoreResult.UNEXPECTED_ERROR;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                STORE_RESULT_HISTOGRAM_NAME, result, CredentialManagerStoreResult.COUNT);
    }
}
