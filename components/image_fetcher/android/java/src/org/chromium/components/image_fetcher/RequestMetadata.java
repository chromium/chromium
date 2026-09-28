// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.net.NetError;

@JNINamespace("image_fetcher")
@NullMarked
public class RequestMetadata {
    public final String mimeType;
    public final int httpResponseCode;
    /**
     * Network error code from the fetch. {@link NetError#OK} means no network error was recorded;
     * it does not imply an HTTP success or that a network request was made (for example, on a cache
     * hit). Negative values identify network or download errors.
     */
    public final @NetError int netError;
    public final String contentLocationHeader; // Corresponds to the fields of C++ RequestMetadata.

    public RequestMetadata(
            String mimeType,
            int httpResponseCode,
            @NetError int netError,
            String contentLocationHeader) {
        this.mimeType = mimeType;
        this.httpResponseCode = httpResponseCode;
        this.netError = netError;
        this.contentLocationHeader = contentLocationHeader;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Image fetcher request metadata: httpResponseCode = ");
        sb.append(httpResponseCode);
        sb.append(", netError = ");
        sb.append(netError);
        sb.append(", mimeType = ");
        sb.append(mimeType);
        sb.append(", contentLocationHeader = ");
        sb.append(contentLocationHeader);
        return sb.toString();
    }
}
