// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher.test;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;

/** A {@link ImageFetcher} for tests that can fetch a test bitmap. */
public class TestImageFetcher extends ImageFetcher.ImageFetcherForTesting {
    private final Bitmap mBitmapToFetch;

    public TestImageFetcher(@Nullable Bitmap bitmapToFetch) {
        mBitmapToFetch = bitmapToFetch;
    }

    @Override
    public void fetchGif(final Params params, Callback<BaseGifImage> callback) {}

    @Override
    public void fetchImage(Params params, Callback<Bitmap> callback) {
        callback.onResult(mBitmapToFetch);
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
