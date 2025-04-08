// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

/**
 * AuthException abstracts away authenticator specific exceptions behind a single interface. It is
 * used for passing information that is useful for better handling of errors.
 */
@NullMarked
public class AuthException extends Exception {
    // TODO(crbug.com/404745044): Remove these fields.
    public static final boolean TRANSIENT = true;
    public static final boolean NONTRANSIENT = false;

    private final GoogleServiceAuthError mAuthError;

    /**
     * Wraps exception that caused auth failure along with transience flag.
     *
     * @param cause Exception that caused auth failure.
     */
    // TODO(crbug.com/404745044): Remove this method.
    public AuthException(boolean isTransient, Exception cause) {
        this(isTransient, "", cause);
    }

    /**
     * Wraps exception that caused auth failure along with transience flag and message.
     *
     * @param message Message describing context in which auth failure happened.
     * @param ex Exception that caused auth failure.
     */
    // TODO(crbug.com/404745044): Remove this method.
    public AuthException(boolean isTransient, String message, Exception ex) {
        super(message, ex);
        mAuthError =
                new GoogleServiceAuthError(
                        isTransient
                                ? GoogleServiceAuthErrorState.CONNECTION_FAILED
                                : GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS);
    }

    /**
     * Wraps exception that caused auth failure and stores the {@link GoogleServiceAuthError}.
     *
     * @param message Message describing context in which auth failure happened.
     * @param ex Exception that caused auth failure.
     * @param error {@link GoogleServiceAuthError} encountered during authentication.
     */
    public AuthException(String message, Exception ex, GoogleServiceAuthError error) {
        super(message, ex);
        mAuthError = error;
    }

    /** Returns the {@link GoogleServiceAuthError} encountered during token fetch. */
    public GoogleServiceAuthError getAuthError() {
        return mAuthError;
    }

    /** Joins messages from all exceptions in the causal chain into a single string. */
    public String stringifyCausalChain() {
        StringBuilder builder = new StringBuilder(toString());
        for (Throwable cause = getCause(); cause != null; cause = cause.getCause()) {
            builder.append("\nCaused by: ").append(cause.toString());
        }
        return builder.toString();
    }
}
