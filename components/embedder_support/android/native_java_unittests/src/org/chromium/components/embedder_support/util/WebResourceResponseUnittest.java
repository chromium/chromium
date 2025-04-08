// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.embedder_support.util;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/**
 * Used by components/embedder_support/android/util/web_resource_response_unittest.cc.
 *
 * @noinspection unused
 */
@NullMarked
@JNINamespace("embedder_support")
class WebResourceResponseUnittest {

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::WebResourceResponse>")
    static @Nullable WebResourceResponseInfo getNullResponse() {
        return null;
    }

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::WebResourceResponse>")
    static @Nullable WebResourceResponseInfo createJavaObject(
            @JniType("std::optional<std::string>") @Nullable String mimeType,
            @JniType("std::optional<std::string>") @Nullable String encoding,
            int statusCode,
            @JniType("std::optional<std::string>") @Nullable String reasonPhrase,
            @JniType("base::flat_map<std::string,std::string>")
                    @Nullable Map<String, String> headers,
            @JniType("std::optional<std::string>") @Nullable String bodyContent) {
        // The actual charset doesn't matter, as we are not in fact testing it..
        ByteArrayInputStream body =
                bodyContent == null
                        ? null
                        : new ByteArrayInputStream(bodyContent.getBytes(StandardCharsets.UTF_8));
        return new WebResourceResponseInfo(
                mimeType, encoding, body, statusCode, reasonPhrase, headers);
    }

    /**
     * Creates a Java object with a null header map.
     *
     * <p>I could not get the JNI generator to handle
     * {@code @JniType("std::optional<base::flat_map<std::string, std::string>>")} correctly, so
     * this variant exists instead.
     */
    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::WebResourceResponse>")
    static @Nullable WebResourceResponseInfo createJavaObject(
            @JniType("std::optional<std::string>") @Nullable String mimeType,
            @JniType("std::optional<std::string>") @Nullable String encoding,
            int statusCode,
            @JniType("std::optional<std::string>") @Nullable String reasonPhrase,
            @JniType("std::optional<std::string>") @Nullable String bodyContent) {
        return createJavaObject(mimeType, encoding, statusCode, reasonPhrase, null, bodyContent);
    }
}
