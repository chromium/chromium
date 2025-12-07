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
    CredentialManagerStoreResult.CUSTOM_ERROR,
    CredentialManagerStoreResult.PROVIDER_CONFIGURATION_ERROR,
    CredentialManagerStoreResult.UNSUPPORTED,
    CredentialManagerStoreResult.PUBLIC_KEY_CREDENTIAL_ERROR,
    CredentialManagerStoreResult.RESTORE_CREDENTIAL_DOM_ERROR,
    CredentialManagerStoreResult.E2EE_UNAVAILABLE_ERROR,
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

    /** An error by the third-party sdk with which was used to make the CreateCredentialRequest. */
    int CUSTOM_ERROR = 6;

    /** Configurations are mismatched for the provider. */
    int PROVIDER_CONFIGURATION_ERROR = 7;

    /**
     * Credential manager is unsupported. It could be disabled on the device so a software update or
     * a restart after enabling may fix this issue. Sometimes hardware may be the limiting factor.
     */
    int UNSUPPORTED = 8;

    /** The operation failed due to a public key credential error. */
    int PUBLIC_KEY_CREDENTIAL_ERROR = 9;

    /** The operation failed due to a restore credential DOM error. */
    int RESTORE_CREDENTIAL_DOM_ERROR = 10;

    /** The operation failed due to E2EE being unavailable. */
    int E2EE_UNAVAILABLE_ERROR = 11;

    int COUNT = 12;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:CredentialManagerStoreResult)
