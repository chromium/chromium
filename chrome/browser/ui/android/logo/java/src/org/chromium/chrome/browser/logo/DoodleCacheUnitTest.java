// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.mock;

import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for the {@link DoodleCache}. */
@RunWith(BaseRobolectricTestRunner.class)
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
                        /* image= */ bitmap,
                        /* darkImage= */ Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888),
                        /* onClickUrl= */ null,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null,
                        /* darkLogUrl= */ null,
                        /* ctaLogUrl= */ null,
                        /* darkCtaLogUrl= */ null);
    }

    @After
    public void tearDown() {
        DoodleCache.setInstanceForTesting(null);
    }

    @Test
    public void testGetForProfile() {
        DoodleCache.setInstanceForTesting(null);
        Profile profile1 = mock(Profile.class);
        Profile profile2 = mock(Profile.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DoodleCache cache1 = DoodleCache.getForProfile(profile1);
                    DoodleCache cache2 = DoodleCache.getForProfile(profile2);
                    assertNotSame(cache1, cache2);
                    assertSame(cache1, DoodleCache.getForProfile(profile1));
                    assertSame(cache2, DoodleCache.getForProfile(profile2));

                    String keyword = "keyword";
                    cache1.updateCachedDoodle(mLogo, keyword);
                    assertEquals(mLogo, cache1.getCachedDoodle(keyword));
                    assertNull(cache2.getCachedDoodle(keyword));
                });
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

        // Advance time by 1 hour + 1 ms.
        mFakeTimeTestRule.advanceMillis(60 * 60 * 1000 + 1);

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
                        /* image= */ bitmap2,
                        /* darkImage= */ Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8),
                        /* onClickUrl= */ null,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null,
                        /* darkLogUrl= */ null,
                        /* ctaLogUrl= */ null,
                        /* darkCtaLogUrl= */ null);

        mDoodleCache.updateCachedDoodle(logo2, keyword);
        assertEquals(logo2, mDoodleCache.getCachedDoodle(keyword));
    }
}
