// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.InputStream;
import java.util.Collections;
import java.util.Map;

/** The response information that is to be returned for a particular resource fetch. */
@JNINamespace("embedder_support")
@NullMarked
public class WebResourceResponseInfo {

    private final @Nullable String mMimeType;
    private final @Nullable String mCharset;
    private @Nullable InputStream mData;
    private final int mStatusCode;
    private final @Nullable String mReasonPhrase;
    private final Map<String, String> mResponseHeaders;

    /**
     * Helps assert that the native code only transfers the stream once. Only modified on the IO
     * thread.
     */
    private boolean mStreamTransferredToNative;

    public WebResourceResponseInfo(
            @Nullable String mimeType, @Nullable String encoding, @Nullable InputStream data) {
        this(
                mimeType,
                encoding,
                data,
                /* statusCode= */ 0,
                /* reasonPhrase= */ null,
                Collections.emptyMap());
    }

    public WebResourceResponseInfo(
            @Nullable String mimeType,
            @Nullable String encoding,
            @Nullable InputStream data,
            int statusCode,
            @Nullable String reasonPhrase,
            @Nullable Map<String, String> responseHeaders) {
        mMimeType = mimeType;
        mCharset = encoding;
        mData = data;
        mStatusCode = statusCode;
        mReasonPhrase = reasonPhrase;
        mResponseHeaders = responseHeaders != null ? responseHeaders : Collections.emptyMap();
    }

    @CalledByNative
    @Nullable
    @JniType("std::optional<std::string>")
    public String getMimeType() {
        return mMimeType;
    }

    @CalledByNative
    @Nullable
    @JniType("std::optional<std::string>")
    public String getCharset() {
        return mCharset;
    }

    @Nullable
    public InputStream getData() {
        return mData;
    }

    @CalledByNative
    private boolean hasInputStream() {
        return mData != null;
    }

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::InputStream>")
    @Nullable
    private InputStream transferStreamToNative() {
        // Only allow to call transferStreamToNative once per object, because this method
        // transfers ownership of the stream and once the unique_ptr<InputStream>
        // is deleted it also closes the original java input stream. This
        // side-effect can result in unexpected behavior, e.g. trying to read
        // from a closed stream.
        assert !mStreamTransferredToNative;
        mStreamTransferredToNative = true;
        InputStream toTransfer = mData;
        mData = null;
        return toTransfer;
    }

    @CalledByNative
    public int getStatusCode() {
        return mStatusCode;
    }

    @CalledByNative
    @JniType("std::optional<std::string>")
    @Nullable
    public String getReasonPhrase() {
        return mReasonPhrase;
    }

    @CalledByNative
    @JniType("base::flat_map<std::string, std::string>")
    public Map<String, String> getResponseHeaders() {
        return mResponseHeaders;
    }
}
