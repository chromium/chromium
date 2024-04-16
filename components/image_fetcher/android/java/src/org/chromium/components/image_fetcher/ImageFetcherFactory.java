// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;

/** Factory to provide the image fetcher best suited for the given config. */
public class ImageFetcherFactory {
    /** Alias for createImageFetcher below. */
    public static ImageFetcher createImageFetcher(
            @ImageFetcherConfig int config, SimpleFactoryKeyHandle simpleFactoryKeyHandle) {
        ImageFetcherBridge bridge =
                ImageFetcherBridge.getForSimpleFactoryKeyHandle(simpleFactoryKeyHandle);
        return createImageFetcher(
                config, bridge, null, InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE);
    }

    /** Alias for createImageFetcher below. */
    public static ImageFetcher createImageFetcher(
            @ImageFetcherConfig int config,
            SimpleFactoryKeyHandle simpleFactoryKeyHandle,
            DiscardableReferencePool discardableReferencePool) {
        ImageFetcherBridge bridge =
                ImageFetcherBridge.getForSimpleFactoryKeyHandle(simpleFactoryKeyHandle);
        return createImageFetcher(
                config,
                bridge,
                discardableReferencePool,
                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE);
    }

    /** Alias for createImageFetcher below. */
    public static ImageFetcher createImageFetcher(
            @ImageFetcherConfig int config,
            SimpleFactoryKeyHandle simpleFactoryKeyHandle,
            DiscardableReferencePool discardableReferencePool,
            int inMemoryCacheSize) {
        ImageFetcherBridge bridge =
                ImageFetcherBridge.getForSimpleFactoryKeyHandle(simpleFactoryKeyHandle);
        return createImageFetcher(config, bridge, discardableReferencePool, inMemoryCacheSize);
    }

    /**
     * Return the image fetcher that matches the given config. This will return an image fetcher
     * config that you must destroy. This function is only used for functions above and tests in
     * this package.
     *
     * @param config The type of ImageFetcher you need.
     * @param imageFetcherBridge Bridge to use.
     * @param discardableReferencePool Used to store images in-memory.
     * @param inMemoryCacheSize The size of the in memory cache (in bytes).
     * @return The correct ImageFetcher according to the provided config.
     */
    static ImageFetcher createImageFetcher(
            @ImageFetcherConfig int config,
            ImageFetcherBridge imageFetcherBridge,
            DiscardableReferencePool discardableReferencePool,
            int inMemoryCacheSize) {
        // TODO(crbug.com/41449848):Allow server-side configuration image fetcher clients.
        switch (config) {
            case ImageFetcherConfig.NETWORK_ONLY:
                return new NetworkImageFetcher(imageFetcherBridge);
            case ImageFetcherConfig.DISK_CACHE_ONLY:
                return new CachedImageFetcher(
                        imageFetcherBridge, new CachedImageFetcher.ImageLoader());
            case ImageFetcherConfig.IN_MEMORY_ONLY:
                assert discardableReferencePool != null;
                return new InMemoryCachedImageFetcher(
                        createImageFetcher(
                                ImageFetcherConfig.NETWORK_ONLY,
                                imageFetcherBridge,
                                discardableReferencePool,
                                inMemoryCacheSize),
                        discardableReferencePool,
                        inMemoryCacheSize);
            case ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE:
                assert discardableReferencePool != null;
                return new InMemoryCachedImageFetcher(
                        createImageFetcher(
                                ImageFetcherConfig.DISK_CACHE_ONLY,
                                imageFetcherBridge,
                                discardableReferencePool,
                                inMemoryCacheSize),
                        discardableReferencePool,
                        inMemoryCacheSize);
            default:
                return null;
        }
    }
}
