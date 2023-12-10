// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Implemented internally.
 *
 * Interface for managing the image cache. This allows
 * native code to call to the image loader across the
 * xsurface.
 */
public interface ImageCacheHelper {
    /**
     * Prefetches the image from the given URL and stores
     * it in the disk cache.
     *
     * @param url URL of image to prefetch
     */
    default void prefetchImage(String url) {}

    /** Clears the image memory cache. */
    default void clearMemoryCache() {}
}
