// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(CredentialManagerStoreResult)
@IntDef({
    CredentialManagerStoreResult.UNEXPECTED_ERROR,
    CredentialManagerStoreResult.SUCCESS,
    CredentialManagerStoreResult.USER_CANCELED,
    CredentialManagerStoreResult.NO_CREATE_OPTIONS,
    CredentialManagerStoreResult.INTERRUPTED,
    CredentialManagerStoreResult.UNKNOWN,
    CredentialManagerStoreResult.COUNT
})
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface CredentialManagerStoreResult {
    /** An error that's not yet explicitly handled in Chrome code. */
    int UNEXPECTED_ERROR = 0;

    /** The operation succeeded. */
    int SUCCESS = 1;

    /** The user intentionally canceled the flow. */
    int USER_CANCELED = 2;

    /** No create options are available from any provider(s). */
    int NO_CREATE_OPTIONS = 3;

    /** The operation failed due to a transient internal API interruption. */
    int INTERRUPTED = 4;

    /** A general catch-all for any other errors, given by the Android API. */
    int UNKNOWN = 5;

    int COUNT = 6;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:CredentialManagerStoreResult)
