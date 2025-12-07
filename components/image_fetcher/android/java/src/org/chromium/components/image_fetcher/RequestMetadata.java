// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("image_fetcher")
@NullMarked
public class RequestMetadata {
    public final String mimeType;
    public final int httpResponseCode;
    public final String contentLocationHeader; // Corresponds to the fields of C++ RequestMetadata.

    public RequestMetadata(String mimeType, int httpResponseCode, String contentLocationHeader) {
        this.mimeType = mimeType;
        this.httpResponseCode = httpResponseCode;
        this.contentLocationHeader = contentLocationHeader;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Image fetcher request metadata: httpResponseCode = ");
        sb.append(httpResponseCode);
        sb.append(", mimeType = ");
        sb.append(mimeType);
        sb.append(", contentLocationHeader = ");
        sb.append(contentLocationHeader);
        return sb.toString();
    }
}
