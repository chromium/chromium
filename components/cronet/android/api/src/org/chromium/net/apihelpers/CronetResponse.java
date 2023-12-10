// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import androidx.annotation.Nullable;

import org.chromium.net.UrlResponseInfo;

import java.util.Objects;

/**
 * A helper object encompassing the headers, body and metadata of a response to Cronet URL
 * requests.
 *
 * @param <T> the response body type
 */
public class CronetResponse<T> {
    /** The headers and other metadata of the response. */
    private final UrlResponseInfo mUrlResponseInfo;

    /** The full body of the response, after performing a user-defined deserialization. */
    private final @Nullable T mResponseBody;

    CronetResponse(UrlResponseInfo urlResponseInfo, @Nullable T responseBody) {
        this.mUrlResponseInfo = urlResponseInfo;
        this.mResponseBody = responseBody;
    }

    /** Returns the headers and other metadata of the response. */
    public UrlResponseInfo getUrlResponseInfo() {
        return mUrlResponseInfo;
    }

    /** Returns the full body of the response, after performing a user-defined deserialization. */
    public @Nullable T getResponseBody() {
        return mResponseBody;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof CronetResponse)) {
            return false;
        }
        CronetResponse<?> that = (CronetResponse<?>) o;
        return Objects.equals(mUrlResponseInfo, that.mUrlResponseInfo)
                && Objects.equals(mResponseBody, that.mResponseBody);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mUrlResponseInfo, mResponseBody);
    }
}
