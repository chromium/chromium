// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@JNINamespace("image_fetcher")
@NullMarked
public class ImageDataFetchResult {
    public final byte[] imageData;
    public final @Nullable RequestMetadata requestMetadata;

    public ImageDataFetchResult(byte[] imageData, @Nullable RequestMetadata requestMetadata) {
        this.imageData = imageData;
        this.requestMetadata = requestMetadata;
    }
}
