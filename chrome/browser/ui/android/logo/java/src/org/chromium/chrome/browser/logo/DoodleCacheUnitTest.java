// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;

/** Unit tests for the {@link DoodleCache}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DoodleCacheUnitTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private DoodleCache mDoodleCache;
    private Logo mLogo;

    @Before
    public void setUp() {
        mDoodleCache = new DoodleCache();
        DoodleCache.setInstanceForTesting(mDoodleCache);
        Bitmap bitmap = Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ALPHA_8);
        mLogo =
                new Logo(
                        bitmap,
                        /* onClickUrl= */ null,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null);
    }

    @After
    public void tearDown() {
        DoodleCache.setInstanceForTesting(null);
    }

    @Test
    public void testCacheAndRetrieve() {
        String keyword = "keyword";
        mDoodleCache.updateCachedDoodle(mLogo, keyword);
        assertEquals(mLogo, mDoodleCache.getCachedDoodle(keyword));
    }

    @Test
    public void testCacheMissKeywordMismatch() {
        String keyword = "keyword";
        mDoodleCache.updateCachedDoodle(mLogo, keyword);
        assertNull(mDoodleCache.getCachedDoodle("other"));
    }

    @Test
    public void testCacheMissNullKeyword() {
        String keyword = "keyword";
        mDoodleCache.updateCachedDoodle(mLogo, keyword);
        assertNull(mDoodleCache.getCachedDoodle(/* searchEngineKeyword= */ null));
    }

    @Test
    public void testCacheExpiration() {
        String keyword = "keyword";
        mDoodleCache.updateCachedDoodle(mLogo, keyword);

        // Advance time by 12 hours + 1 ms.
        mFakeTimeTestRule.advanceMillis(12 * 60 * 60 * 1000 + 1);

        assertNull(mDoodleCache.getCachedDoodle(keyword));
    }

    @Test
    public void testUpdateReplacesCache() {
        String keyword = "keyword";
        mDoodleCache.updateCachedDoodle(mLogo, keyword);

        Bitmap bitmap2 =
                Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
        Logo logo2 =
                new Logo(
                        bitmap2,
                        /* onClickUrl= */ null,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null);

        mDoodleCache.updateCachedDoodle(logo2, keyword);
        assertEquals(logo2, mDoodleCache.getCachedDoodle(keyword));
    }
}
