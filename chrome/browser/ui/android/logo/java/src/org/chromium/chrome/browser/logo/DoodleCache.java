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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/**
 * Caches the search provider doodle to avoid unnecessary network requests and animations. Keyed by
 * {@link Profile} to maintain state across different NTP instances within the same profile.
 */
@NullMarked
public class DoodleCache {
    private static final ProfileKeyedMap<DoodleCache> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.noRequiredCleanupAction());
    private static @Nullable DoodleCache sInstanceForTesting;

    private @Nullable Logo mCachedDoodle;
    private long mCachedDoodleTimeMillis;
    private @Nullable String mCachedDoodleKeyword;
    // 1 hour cache validity.
    private static final long CACHE_TTL_MS = 60 * 60 * 1000;

    /** Returns the {@link DoodleCache} instance for the given {@link Profile}. */
    public static DoodleCache getForProfile(Profile profile) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sProfileMap.getForProfile(profile, p -> new DoodleCache());
    }

    /** Sets a DoodleCache instance for testing. */
    public static void setInstanceForTesting(@Nullable DoodleCache instance) {
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
