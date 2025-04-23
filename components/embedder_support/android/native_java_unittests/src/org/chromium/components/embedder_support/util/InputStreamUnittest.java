// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.io.InputStream;

/**
 * Used by components/embedder_support/android/util/input_stream_unittest.cc
 *
 * @noinspection unused
 */
@SuppressWarnings("InputStreamSlowMultibyteRead")
@NullMarked
class InputStreamUnittest {
    private InputStreamUnittest() {}

    @CalledByNative
    @Nullable
    @JniType("std::unique_ptr<embedder_support::InputStream>")
    static InputStream getNullStream() {
        return null;
    }

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::InputStream>")
    static InputStream getEmptyStream() {
        return new InputStream() {
            @Override
            public int read() {
                return -1;
            }
        };
    }

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::InputStream>")
    static InputStream getThrowingStream() {
        return new InputStream() {
            @Override
            public int available() throws IOException {
                throw new IOException();
            }

            @Override
            public void close() throws IOException {
                throw new IOException();
            }

            @Override
            public long skip(long n) throws IOException {
                throw new IOException();
            }

            @Override
            public int read() throws IOException {
                throw new IOException();
            }
        };
    }

    @CalledByNative
    @JniType("std::unique_ptr<embedder_support::InputStream>")
    static InputStream getCountingStream(final int size) {
        return new InputStream() {
            private int mCount;

            @Override
            public int read() {
                if (mCount < size) {
                    return mCount++ % 256;
                } else {
                    return -1;
                }
            }
        };
    }
}
