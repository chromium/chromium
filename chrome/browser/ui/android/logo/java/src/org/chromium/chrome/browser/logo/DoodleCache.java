// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;

/**
 * Caches the search provider doodle to avoid unnecessary network requests and animations. Singleton
 * class to maintain state across different NTP instances.
 */
@NullMarked
public class DoodleCache {
    private static @Nullable DoodleCache sInstanceForTesting;

    private @Nullable Logo mCachedDoodle;
    private long mCachedDoodleTimeMillis;
    private @Nullable String mCachedDoodleKeyword;
    // 12 hours cache validity.
    private static final long CACHE_TTL_MS = 60 * 60 * 1000;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final DoodleCache sInstance = new DoodleCache();
    }

    /** Returns the singleton instance of DoodleCache. */
    public static DoodleCache getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.sInstance;
    }

    /** Sets a DoodleCache instance for testing. */
    public static void setInstanceForTesting(DoodleCache instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    @VisibleForTesting
    DoodleCache() {}

    /**
     * Returns the cached doodle if it is valid and matches the given keyword.
     *
     * @param searchEngineKeyword The keyword of the current search engine.
     * @return The cached Logo, or null if invalid/expired.
     */
    public @Nullable Logo getCachedDoodle(@Nullable String searchEngineKeyword) {
        if (mCachedDoodle != null
                && searchEngineKeyword != null
                && searchEngineKeyword.equals(mCachedDoodleKeyword)
                && (TimeUtils.currentTimeMillis() - mCachedDoodleTimeMillis) < CACHE_TTL_MS) {
            return mCachedDoodle;
        }
        return null;
    }

    /**
     * Updates the cached doodle.
     *
     * @param doodle The new doodle to cache.
     * @param searchEngineKeyword The keyword of the search engine associated with the doodle.
     */
    public void updateCachedDoodle(Logo doodle, @Nullable String searchEngineKeyword) {
        mCachedDoodle = doodle;
        mCachedDoodleTimeMillis = TimeUtils.currentTimeMillis();
        mCachedDoodleKeyword = searchEngineKeyword;
    }
}
