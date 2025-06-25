// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import android.graphics.Bitmap;

import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@JNINamespace("image_fetcher")
@NullMarked
public class ImageFetchResult {
    public final @Nullable Bitmap imageBitmap;
    public final RequestMetadata requestMetadata;

    public ImageFetchResult(@Nullable Bitmap imageBitmap, RequestMetadata requestMetadata) {
        this.imageBitmap = imageBitmap;
        this.requestMetadata = requestMetadata;
    }
}
