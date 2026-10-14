// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertEquals;

import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Checks that the settings avatar sizes with an AI tier ring leave exactly enough room for the
 * ring, so the visible avatar keeps its original size (in dp).
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AiTierRingDimensTest {
    private Resources mResources;
    private int mRingPadding;

    @Before
    public void setUp() {
        mResources = ApplicationProvider.getApplicationContext().getResources();
        mRingPadding =
                mResources.getDimensionPixelSize(
                                org.chromium.chrome.browser.signin.services.R.dimen
                                        .ai_tier_ring_thickness_large)
                        + mResources.getDimensionPixelSize(
                                org.chromium.chrome.browser.signin.services.R.dimen
                                        .ai_tier_ring_spacing);
    }

    @Test
    public void testMainSettingsAvatarSize() {
        assertEquals(
                mResources.getDimensionPixelSize(
                                org.chromium.chrome.browser.signin.services.R.dimen
                                        .user_picture_size)
                        + 2 * mRingPadding,
                mResources.getDimensionPixelSize(
                        org.chromium.chrome.browser.signin.services.R.dimen
                                .user_picture_size_with_ai_tier_ring));
    }

    @Test
    public void testAccountPageAvatarSize() {
        assertEquals(
                mResources.getDimensionPixelSize(R.dimen.sync_promo_account_image_size)
                        + 2 * mRingPadding,
                mResources.getDimensionPixelSize(
                        R.dimen.sync_promo_account_image_size_with_ai_tier_ring));
    }
}
