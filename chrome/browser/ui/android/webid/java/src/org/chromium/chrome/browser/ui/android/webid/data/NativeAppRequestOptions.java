// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * Everything a native identity provider application needs in order to take over both halves of an
 * active mode FedCM request: rendering the account chooser, and issuing the assertion token that
 * the id_assertion_endpoint would otherwise have returned.
 *
 * <p>{@link #getAssertionParams()} carries the request parameters destined for the IdP. It is
 * produced by the same code that builds the id_assertion_endpoint POST body, so a native
 * application receives exactly what an HTTP endpoint would and the two paths cannot drift as new
 * request parameters are added.
 *
 * <p>Anything the application needs in order to render its account chooser (the requested fields,
 * for instance) is in there too, so it is deliberately not duplicated as a separate member.
 */
@NullMarked
public class NativeAppRequestOptions {
    private final GURL mConfigUrl;
    private final String mRpOrigin;
    private final String mAssertionParams;
    private final String mLoginHint;
    private final String mDomainHint;

    @CalledByNative
    public NativeAppRequestOptions(
            @JniType("GURL") GURL configUrl,
            @JniType("std::string") String rpOrigin,
            @JniType("std::string") String assertionParams,
            @JniType("std::string") String loginHint,
            @JniType("std::string") String domainHint) {
        mConfigUrl = configUrl;
        mRpOrigin = rpOrigin;
        mAssertionParams = assertionParams;
        mLoginHint = loginHint;
        mDomainHint = domainHint;
    }

    /**
     * The IdP config URL. Identifies which IdP this request is for and is the origin that the
     * application's Digital Asset Links are verified against.
     */
    public GURL getConfigUrl() {
        return mConfigUrl;
    }

    /**
     * The top frame (embedding) origin of the page that invoked FedCM. This matches the origin used
     * for the browser's own IdP network requests.
     */
    public String getRpOrigin() {
        return mRpOrigin;
    }

    /**
     * URL-encoded id_assertion_endpoint request parameters (client_id, nonce, mode, fields, params,
     * type, ...).
     */
    public String getAssertionParams() {
        return mAssertionParams;
    }

    /**
     * Account filtering hint. Per spec this is used by the user agent and is not forwarded to the
     * IdP, so it is deliberately not part of {@link #getAssertionParams()}. Empty when unspecified
     * by the relying party.
     */
    public String getLoginHint() {
        return mLoginHint;
    }

    /** Account filtering hint. See {@link #getLoginHint()}. */
    public String getDomainHint() {
        return mDomainHint;
    }
}
