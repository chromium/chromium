// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;

/** Provides access to native implementations of ImageFetcher for the given browser context. */
@JNINamespace("image_fetcher")
public class ImageFetcherBridge {
    private final SimpleFactoryKeyHandle mSimpleFactoryKeyHandle;

    /**
     * Get the ImageFetcherBridge for the given browser context.
     *
     * @param simpleFactoryKeyHandle   The SimpleFactoryKeyHandle for which the ImageFetcherBridge
     *         is returned.
     * @return The ImageFetcherBridge for the given browser context.
     */
    public static ImageFetcherBridge getForSimpleFactoryKeyHandle(
            SimpleFactoryKeyHandle simpleFactoryKeyHandle) {
        ThreadUtils.assertOnUiThread();

        return new ImageFetcherBridge(simpleFactoryKeyHandle);
    }

    /**
     * Creates a ImageFetcherBridge for the given browser context.
     *
     * @param SimpleFactoryKeyHandle The SimpleFactoryKeyHandle for which the ImageFetcherBridge is
     *         returned.
     */
    @VisibleForTesting
    ImageFetcherBridge(SimpleFactoryKeyHandle simpleFactoryKeyHandle) {
        mSimpleFactoryKeyHandle = simpleFactoryKeyHandle;
    }

    /**
     * Get the full path of the given url on disk.
     *
     * @param url The url to hash.
     * @return The full path to the resource on disk.
     */
    public String getFilePath(String url) {
        return ImageFetcherBridgeJni.get().getFilePath(mSimpleFactoryKeyHandle, url);
    }

    /**
     * Fetch a gif from native or null if the gif can't be fetched or decoded.
     *
     * @param config The configuration of the image fetcher.
     * @param params The parameters to specify image fetching details.
     * @param callback The callback to call when the gif is ready. The callback will be invoked on
     *      the same thread it was called on.
     */
    public void fetchGif(
            @ImageFetcherConfig int config,
            final ImageFetcher.Params params,
            Callback<BaseGifImage> callback) {
        ImageFetcherBridgeJni.get()
                .fetchImageData(
                        mSimpleFactoryKeyHandle,
                        config,
                        params.url,
                        params.clientName,
                        params.expirationIntervalMinutes,
                        (byte[] data) -> {
                            if (data == null || data.length == 0) {
                                callback.onResult(null);
                                return;
                            }

                            callback.onResult(new BaseGifImage(data));
                        });
    }

    /**
     * Fetch the image from native, then resize it to the given dimensions.
     *
     * @param config The configuration of the image fetcher.
     * @param params The parameters to specify image fetching details.
     * @param callback The callback to call when the image is ready. The callback will be invoked on
     *      the same thread that it was called on.
     */
    public void fetchImage(
            @ImageFetcherConfig int config,
            final ImageFetcher.Params params,
            Callback<Bitmap> callback) {
        ImageFetcherBridgeJni.get()
                .fetchImage(
                        mSimpleFactoryKeyHandle,
                        config,
                        params.url,
                        params.clientName,
                        params.width,
                        params.height,
                        params.expirationIntervalMinutes,
                        (bitmap) -> {
                            if (params.shouldResize) {
                                callback.onResult(
                                        ImageFetcher.resizeImage(
                                                bitmap, params.width, params.height));
                            } else {
                                callback.onResult(bitmap);
                            }
                        });
    }

    /**
     * Report a metrics event.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param eventId The event to report.
     */
    public void reportEvent(String clientName, @ImageFetcherEvent int eventId) {
        ImageFetcherBridgeJni.get().reportEvent(clientName, eventId);
    }

    /**
     * Report a timing event for a cache hit.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     *      total duration.
     */
    public void reportCacheHitTime(String clientName, long startTimeMillis) {
        ImageFetcherBridgeJni.get().reportCacheHitTime(clientName, startTimeMillis);
    }

    /**
     * Report a timing event for a call to native
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     *      total duration.
     */
    public void reportTotalFetchTimeFromNative(String clientName, long startTimeMillis) {
        ImageFetcherBridgeJni.get().reportTotalFetchTimeFromNative(clientName, startTimeMillis);
    }

    @NativeMethods
    interface Natives {
        // Native methods
        String getFilePath(SimpleFactoryKeyHandle simpleFactoryKeyHandle, String url);

        void fetchImageData(
                SimpleFactoryKeyHandle simpleFactoryKeyHandle,
                @ImageFetcherConfig int config,
                String url,
                String clientName,
                int expirationIntervalMinutes,
                Callback<byte[]> callback);

        void fetchImage(
                SimpleFactoryKeyHandle simpleFactoryKeyHandle,
                @ImageFetcherConfig int config,
                String url,
                String clientName,
                int frameWidth,
                int frameHeight,
                int expirationIntervalMinutes,
                Callback<Bitmap> callback);

        void reportEvent(String clientName, int eventId);

        void reportCacheHitTime(String clientName, long startTimeMillis);

        void reportTotalFetchTimeFromNative(String clientName, long startTimeMillis);
    }
}
