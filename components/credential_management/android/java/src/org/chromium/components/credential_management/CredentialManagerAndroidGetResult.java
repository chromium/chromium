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
// LINT.IfChange(CredentialManagerAndroidGetResult)
@IntDef({
    CredentialManagerAndroidGetResult.UNEXPECTED_ERROR,
    CredentialManagerAndroidGetResult.SUCCESS,
    CredentialManagerAndroidGetResult.USER_CANCELED,
    CredentialManagerAndroidGetResult.CUSTOM_ERROR,
    CredentialManagerAndroidGetResult.INTERRUPTED,
    CredentialManagerAndroidGetResult.PROVIDER_CONFIGURATION_ERROR,
    CredentialManagerAndroidGetResult.UNKNOWN,
    CredentialManagerAndroidGetResult.UNSUPPORTED,
    CredentialManagerAndroidGetResult.PUBLIC_KEY_CREDENTIAL_ERROR,
    CredentialManagerAndroidGetResult.NO_CREDENTIAL,
    CredentialManagerAndroidGetResult.COUNT
})
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface CredentialManagerAndroidGetResult {
    /** An error that's not yet explicitly handled in Chrome code. */
    int UNEXPECTED_ERROR = 0;

    /** The operation succeeded. */
    int SUCCESS = 1;

    /** The user intentionally canceled the flow. */
    int USER_CANCELED = 2;

    /** An error by the third-party sdk with which was used to make the GetCredentialRequest. */
    int CUSTOM_ERROR = 3;

    /** The operation failed due to a transient internal API interruption. */
    int INTERRUPTED = 4;

    /** Configurations are mismatched for the provider. */
    int PROVIDER_CONFIGURATION_ERROR = 5;

    /** Get credential operation failed with no more detailed information. */
    int UNKNOWN = 6;

    /**
     * Credential manager is unsupported. It could be disabled on the device so a software update or
     * a restart after enabling may fix this issue. Sometimes hardware may be the limiting factor.
     */
    int UNSUPPORTED = 7;

    /** The operation failed due to a public key credential error. */
    int PUBLIC_KEY_CREDENTIAL_ERROR = 8;

    /** The operation failed due to no credential being found. */
    int NO_CREDENTIAL = 9;

    int COUNT = 10;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:CredentialManagerAndroidGetResult)
