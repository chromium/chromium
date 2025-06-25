// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher.test;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.image_fetcher.ImageDataFetchResult;
import org.chromium.components.image_fetcher.ImageFetchResult;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.RequestMetadata;

/** A {@link ImageFetcher} for tests that can fetch a test bitmap. */
public class TestImageFetcher extends ImageFetcher.ImageFetcherForTesting {
    private final Bitmap mBitmap;
    private final ImageFetchResult mImageFetchResult;

    public TestImageFetcher(
            @Nullable Bitmap bitmapToFetch, @Nullable RequestMetadata bitmapFetchRequestMetadata) {
        mBitmap = bitmapToFetch;
        mImageFetchResult = new ImageFetchResult(bitmapToFetch, bitmapFetchRequestMetadata);
    }

    @Override
    public void fetchGif(final Params params, Callback<ImageDataFetchResult> callback) {}

    @Override
    public void fetchImage(Params params, Callback<Bitmap> callback) {
        callback.onResult(mBitmap);
    }

    @Override
    public void fetchImageWithRequestMetadata(Params params, Callback<ImageFetchResult> callback) {
        callback.onResult(mImageFetchResult);
    }

    @Override
    public void clear() {}

    @Override
    public @ImageFetcherConfig int getConfig() {
        return ImageFetcherConfig.IN_MEMORY_ONLY;
    }

    @Override
    public void destroy() {}
}
