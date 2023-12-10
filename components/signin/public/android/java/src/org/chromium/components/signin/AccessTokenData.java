// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 * AccessTokenData encapsulates result of getToken method call from GoogleAuthUtil. It is a
 * holder that contains the access token and its expiration time.
 */
public class AccessTokenData {
    /** The expiration time value when there's no known expiration time for the token. */
    public static final long NO_KNOWN_EXPIRATION_TIME = 0;

    private final String mToken;
    private final long mExpirationTimeSecs;

    /**
     * Construct an access token data with its value and its expiration time in seconds.
     * @param token the value of the access token.
     * @param expirationTimeSecs the number of seconds (NOT milliseconds) after the Unix epoch when
     *         the token is scheduled to expire. It is set to 0 if there's no known expiration time.
     */
    public AccessTokenData(String token, long expirationTimeSecs) {
        assert token != null;
        this.mToken = token;
        this.mExpirationTimeSecs = expirationTimeSecs;
    }

    /**
     * Constructor for when there's no known expiration time, set the expiration time to 0.
     * @param token the string value of the access token.
     */
    public AccessTokenData(String token) {
        this(token, NO_KNOWN_EXPIRATION_TIME);
    }

    /** Returns the value of the access token. */
    public String getToken() {
        return this.mToken;
    }

    /**
     * Returns the number of seconds (NOT milliseconds) after the Unix epoch when the token is
     * scheduled to expire. Returns 0 if there's no known expiration time.
     */
    public long getExpirationTimeSecs() {
        return this.mExpirationTimeSecs;
    }
}
