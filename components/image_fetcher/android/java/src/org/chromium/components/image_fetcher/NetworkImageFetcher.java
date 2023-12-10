// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import android.graphics.Bitmap;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;

/** Image Fetcher implementation that fetches from the network. */
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
    public void fetchGif(final ImageFetcher.Params params, Callback<BaseGifImage> callback) {
        getImageFetcherBridge().fetchGif(getConfig(), params, callback);
    }

    @Override
    public void fetchImage(final Params params, Callback<Bitmap> callback) {
        long startTimeMillis = System.currentTimeMillis();
        getImageFetcherBridge()
                .fetchImage(
                        getConfig(),
                        params,
                        (Bitmap bitmapFromNative) -> {
                            callback.onResult(bitmapFromNative);
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
