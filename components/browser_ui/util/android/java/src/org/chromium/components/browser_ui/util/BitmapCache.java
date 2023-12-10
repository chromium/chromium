// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.graphics.Bitmap;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.collection.LruCache;

import org.chromium.base.CollectionUtil;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * In-memory cache of Bitmap.
 *
 * Bitmaps are cached in memory and shared across all instances of BitmapCache. There are two
 * levels of caches: one static cache for deduplication (or canonicalization) of bitmaps, and one
 * per-object cache for storing recently used bitmaps. The deduplication cache uses weak references
 * to allow bitmaps to be garbage-collected once they are no longer in use. As long as there is at
 * least one strong reference to a bitmap, it is not going to be GC'd and will therefore stay in the
 * cache. This ensures that there is never more than one (reachable) copy of a bitmap in memory.
 * The {@link RecentlyUsedCache} is limited in size and dropped under memory pressure, or when the
 * object is destroyed.
 */
public class BitmapCache {
    private final int mCacheSize;

    /**
     * Least-recently-used cache that falls back to the deduplication cache on misses.
     * This propagates bitmaps that were only in the deduplication cache back into the LRU cache
     * and also moves them to the front to ensure correct eviction order.
     */
    private static class RecentlyUsedCache extends LruCache<String, Bitmap> {
        private RecentlyUsedCache(int size) {
            super(size);
        }

        @Override
        protected Bitmap create(String key) {
            WeakReference<Bitmap> cachedBitmap = sDeduplicationCache.get(key);
            return cachedBitmap == null ? null : cachedBitmap.get();
        }

        @Override
        protected int sizeOf(String key, Bitmap thumbnail) {
            return thumbnail.getByteCount();
        }
    }

    /**
     * Discardable reference to the {@link RecentlyUsedCache} that can be dropped under memory
     * pressure.
     */
    private DiscardableReferencePool.DiscardableReference<RecentlyUsedCache> mBitmapCache;

    /**
     * The reference pool that contains the {@link #mBitmapCache}. Used to recreate a new cache
     * after the old one has been dropped.
     */
    private final DiscardableReferencePool mReferencePool;

    /**
     * Static cache used for deduplicating bitmaps. The key is a pair of file name and thumbnail
     * size (as for the {@link #mBitmapCache}.
     */
    private static Map<String, WeakReference<Bitmap>> sDeduplicationCache = new HashMap<>();

    private static int sUsageCount;

    /**
     * Creates an instance of a {@link BitmapCache}.
     *
     * This constructor must be called on UI thread.
     *
     * @param referencePool The discardable reference pool. Typically this should be the
     *                      {@link DiscardableReferencePool} for the application.
     * @param size The capacity of the cache in bytes.
     */
    public BitmapCache(DiscardableReferencePool referencePool, int size) {
        ThreadUtils.assertOnUiThread();
        mReferencePool = referencePool;
        mCacheSize = size;
        mBitmapCache = referencePool.put(new RecentlyUsedCache(mCacheSize));
    }

    /** Manually destroy the BitmapCache. */
    public void destroy() {
        assert mReferencePool != null;
        assert mBitmapCache != null;
        mReferencePool.remove(mBitmapCache);
        mBitmapCache = null;
    }

    public Bitmap getBitmap(String key) {
        ThreadUtils.assertOnUiThread();
        if (mBitmapCache == null) return null;

        Bitmap cachedBitmap = getBitmapCache().get(key);
        assert cachedBitmap == null || !cachedBitmap.isRecycled();
        maybeScheduleDeduplicationCache();
        return cachedBitmap;
    }

    public void putBitmap(@NonNull String key, @Nullable Bitmap bitmap) {
        ThreadUtils.assertOnUiThread();
        if (bitmap == null || mBitmapCache == null) return;

        if (!SysUtils.isLowEndDevice()) getBitmapCache().put(key, bitmap);
        maybeScheduleDeduplicationCache();
        sDeduplicationCache.put(key, new WeakReference<>(bitmap));
    }

    /** Evict all bitmaps from the cache. */
    public void clear() {
        getBitmapCache().evictAll();
        scheduleDeduplicationCache();
    }

    /** @return The total number of bytes taken by the bitmaps in this cache. */
    public int size() {
        return getBitmapCache().size();
    }

    private RecentlyUsedCache getBitmapCache() {
        RecentlyUsedCache bitmapCache = mBitmapCache.get();
        if (bitmapCache == null) {
            bitmapCache = new RecentlyUsedCache(mCacheSize);
            mBitmapCache = mReferencePool.put(bitmapCache);
        }
        return bitmapCache;
    }

    private static void maybeScheduleDeduplicationCache() {
        sUsageCount++;
        // Amortized cost of automatic dedup work is constant.
        if (sUsageCount < sDeduplicationCache.size()) return;
        sUsageCount = 0;
        scheduleDeduplicationCache();
    }

    private static void scheduleDeduplicationCache() {
        Looper.myQueue()
                .addIdleHandler(
                        () -> {
                            compactDeduplicationCache();
                            return false;
                        });
    }

    /**
     * Compacts the deduplication cache by removing all entries that have been cleared by the
     * garbage collector.
     */
    private static void compactDeduplicationCache() {
        CollectionUtil.strengthen(sDeduplicationCache.values());
    }

    static void clearDedupCacheForTesting() {
        sDeduplicationCache.clear();
    }

    static int dedupCacheSizeForTesting() {
        return sDeduplicationCache.size();
    }
}
