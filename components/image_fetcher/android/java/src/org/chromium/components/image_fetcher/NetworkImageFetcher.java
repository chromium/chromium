// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Image Fetcher implementation that fetches from the network. */
@NullMarked
public class NetworkImageFetcher extends ImageFetcher {
    /**
     * Creates a NetworkImageFetcher.
     *
     * @param imageFetcherBridge Bridge used to interact with native.
     */
    NetworkImageFetcher(ImageFetcherBridge imageFetcherBridge) {
        super(imageFetcherBridge);
    }

    @Override
    public void destroy() {
        // Do nothing, this lives for the lifetime of the application.
    }

    @Override
    public void fetchGif(
            final ImageFetcher.Params params, Callback<ImageDataFetchResult> callback) {
        getImageFetcherBridge().fetchGif(getConfig(), params, callback);
    }

    @Override
    public void fetchImage(final Params params, Callback<@Nullable Bitmap> callback) {
        long startTimeMillis = System.currentTimeMillis();
        getImageFetcherBridge()
                .fetchImage(
                        getConfig(),
                        params,
                        (@Nullable Bitmap bitmapFromNative) -> {
                            callback.onResult(bitmapFromNative);
                            getImageFetcherBridge()
                                    .reportTotalFetchTimeFromNative(
                                            params.clientName, startTimeMillis);
                        });
    }

    @Override
    public void fetchImageWithRequestMetadata(
            final Params params, Callback<ImageFetchResult> callback) {
        long startTimeMillis = System.currentTimeMillis();
        getImageFetcherBridge()
                .fetchImageWithRequestMetadata(
                        getConfig(),
                        params,
                        (ImageFetchResult bitmapFromNativeFetchResult) -> {
                            callback.onResult(bitmapFromNativeFetchResult);
                            getImageFetcherBridge()
                                    .reportTotalFetchTimeFromNative(
                                            params.clientName, startTimeMillis);
                        });
    }

    @Override
    public void clear() {}

    @Override
    public @ImageFetcherConfig int getConfig() {
        return ImageFetcherConfig.NETWORK_ONLY;
    }
}
