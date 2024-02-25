// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;

/** Test for ImageFetcherFactory. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherFactoryTest {
    @Mock ImageFetcherBridge mImageFetcherBridge;
    @Mock DiscardableReferencePool mReferencePool;
    @Mock SimpleFactoryKeyHandle mSimpleFactoryKeyHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testGetImageFetcher() {
        assertEquals(
                ImageFetcherConfig.NETWORK_ONLY,
                ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.NETWORK_ONLY,
                                mImageFetcherBridge,
                                mReferencePool,
                                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(
                ImageFetcherConfig.DISK_CACHE_ONLY,
                ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.DISK_CACHE_ONLY,
                                mImageFetcherBridge,
                                mReferencePool,
                                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(
                ImageFetcherConfig.IN_MEMORY_ONLY,
                ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.IN_MEMORY_ONLY,
                                mImageFetcherBridge,
                                mReferencePool,
                                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
        assertEquals(
                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                mImageFetcherBridge,
                                mReferencePool,
                                InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE)
                        .getConfig());
    }

    @Test
    @SmallTest
    public void testCreateImageFetcher() {
        int config = ImageFetcherConfig.NETWORK_ONLY;

        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(config, mSimpleFactoryKeyHandle);
        assertNotNull(imageFetcher);
        assertNotEquals(mImageFetcherBridge, imageFetcher.getImageFetcherBridge());

        ImageFetcher imageFetcherWithRefPool =
                ImageFetcherFactory.createImageFetcher(
                        config, mSimpleFactoryKeyHandle, mReferencePool);
        assertNotNull(imageFetcherWithRefPool);
        assertNotEquals(mImageFetcherBridge, imageFetcherWithRefPool.getImageFetcherBridge());

        ImageFetcher imageFetcherWithRefPoolAndCacheSize =
                ImageFetcherFactory.createImageFetcher(
                        config,
                        mSimpleFactoryKeyHandle,
                        mReferencePool,
                        InMemoryCachedImageFetcher.DEFAULT_CACHE_SIZE);
        assertNotNull(imageFetcherWithRefPoolAndCacheSize);
        assertNotEquals(
                mImageFetcherBridge, imageFetcherWithRefPoolAndCacheSize.getImageFetcherBridge());
    }
}
