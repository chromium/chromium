// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

/**
 * The callback when an image is downloaded. This class is different with
 * {@link ImageDownloadCallback} and is only used by {@link MediaImageManager}.
 */
public interface MediaImageCallback {
    /**
     * Called when image downloading is complete.
     * @param bitmap The downloaded image. |null| indicates there is no available src for download
     * or image download failed.
     */
    void onImageDownloaded(@Nullable Bitmap image);
}
