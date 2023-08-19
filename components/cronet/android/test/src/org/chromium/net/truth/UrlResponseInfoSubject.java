// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.truth;

import static com.google.common.truth.Fact.simpleFact;
import static com.google.common.truth.Truth.assertAbout;

import androidx.annotation.Nullable;

import com.google.common.truth.FailureMetadata;
import com.google.common.truth.IntegerSubject;
import com.google.common.truth.IterableSubject;
import com.google.common.truth.LongSubject;
import com.google.common.truth.MapSubject;
import com.google.common.truth.StringSubject;
import com.google.common.truth.Subject;

import org.chromium.net.UrlResponseInfo;

/**
 * A custom Truth subject for Cronet's {@link UrlResponseInfo}. This is used
 * to assert on the info methods for custom error messages.
 * Example usage:
 * <pre>
 * // to assert the info's url string is not empty.
 * assertThat(info).hasUrlThat().isNotEmpty();
 * </pre>
 */
public class UrlResponseInfoSubject extends Subject {
    private final UrlResponseInfo mActual;

    protected UrlResponseInfoSubject(FailureMetadata metadata, UrlResponseInfo subject) {
        super(metadata, subject);
        mActual = subject;
    }

    public static UrlResponseInfoSubject assertThat(@Nullable UrlResponseInfo info) {
        return assertAbout(responseInfos()).that(info);
    }

    public static Subject.Factory<UrlResponseInfoSubject, UrlResponseInfo> responseInfos() {
        return UrlResponseInfoSubject::new;
    }

    // Test assertions
    public IntegerSubject hasHttpStatusCodeThat() {
        assertNotNull();
        return check("getHttpStatusCode()").that(mActual.getHttpStatusCode());
    }

    public StringSubject hasHttpStatusTextThat() {
        assertNotNull();
        return check("getHttpStatusText()").that(mActual.getHttpStatusText());
    }

    public IterableSubject hasHeadersListThat() {
        assertNotNull();
        return check("getAllHeadersAsList()").that(mActual.getAllHeadersAsList());
    }

    public MapSubject hasHeadersThat() {
        assertNotNull();
        return check("getAllHeaders()").that(mActual.getAllHeaders());
    }

    public StringSubject hasNegotiatedProtocolThat() {
        assertNotNull();
        return check("getNegotiatedProtocol()").that(mActual.getNegotiatedProtocol());
    }

    public StringSubject hasProxyServerThat() {
        assertNotNull();
        return check("getProxyServer()").that(mActual.getProxyServer());
    }

    public LongSubject hasReceivedByteCountThat() {
        assertNotNull();
        return check("getReceivedByteCount()").that(mActual.getReceivedByteCount());
    }

    public StringSubject hasUrlThat() {
        assertNotNull();
        return check("getUrl()").that(mActual.getUrl());
    }

    public IterableSubject hasUrlChainThat() {
        assertNotNull();
        return check("getUrlChain()").that(mActual.getUrlChain());
    }

    public void wasCached() {
        assertNotNull();
        if (!mActual.wasCached()) {
            failWithoutActual(simpleFact("responseInfo expected to be cached"));
        }
    }

    public void wasNotCached() {
        assertNotNull();
        if (mActual.wasCached()) {
            failWithoutActual(simpleFact("responseInfo expected not to be cached"));
        }
    }

    // Checks that the actual ResponseInfoSubject is not null before doing any other assertions.
    // This was not done in the assertThat method because the user may still
    // assertThat(info).isNull().
    private void assertNotNull() {
        if (mActual == null) {
            failWithoutActual(simpleFact("the responseInfo is null"));
        }
    }
}
