// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AvatarGenerator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AvatarGeneratorUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @Test
    public void testMakeRoundAvatar_nullAvatarReturnsNull() {
        assertNull(AvatarGenerator.makeRoundAvatar(mContext.getResources(), (Bitmap) null, 80));
    }

    @Test
    public void testMakeRoundAvatar_emptyListReturnsNull() {
        assertNull(
                AvatarGenerator.makeRoundAvatar(
                        mContext.getResources(), Collections.emptyList(), 80));
    }

    @Test
    public void testMakeRoundAvatar_listWithNullReturnsNull() {
        List<Bitmap> avatars =
                Arrays.asList(Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888), null);
        assertNull(AvatarGenerator.makeRoundAvatar(mContext.getResources(), avatars, 80));
    }

    @Test
    public void testMakeRoundAvatar_singleAvatarReturnsDrawable() {
        Bitmap avatar = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        avatar.eraseColor(Color.BLUE);

        int imageSize = 40;
        Drawable drawable =
                AvatarGenerator.makeRoundAvatar(mContext.getResources(), avatar, imageSize);
        assertNotNull(drawable);
        assertTrue(drawable instanceof RoundedBitmapDrawable);
        Bitmap bitmap = ((RoundedBitmapDrawable) drawable).getBitmap();
        // The backing bitmap is scaled by SUPERSAMPLING_FACTOR for supersampling.
        int expectedBitmapSize = imageSize * AvatarGenerator.SUPERSAMPLING_FACTOR;
        assertEquals(expectedBitmapSize, bitmap.getWidth());
        assertEquals(expectedBitmapSize, bitmap.getHeight());
        // The intrinsic drawable dimensions remain 1x (imageSize).
        assertEquals(imageSize, drawable.getIntrinsicWidth());
        assertEquals(imageSize, drawable.getIntrinsicHeight());
    }

    @Test
    public void testMakeRoundAvatar_multipleAvatars() {
        Bitmap avatar1 = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        Bitmap avatar2 = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        avatar1.eraseColor(Color.RED);
        avatar2.eraseColor(Color.GREEN);

        int imageSize = 40;
        Drawable drawable =
                AvatarGenerator.makeRoundAvatar(
                        mContext.getResources(), Arrays.asList(avatar1, avatar2), imageSize);
        assertNotNull(drawable);
        assertTrue(drawable instanceof RoundedBitmapDrawable);
        Bitmap bitmap = ((RoundedBitmapDrawable) drawable).getBitmap();
        // The backing bitmap is scaled by SUPERSAMPLING_FACTOR for supersampling.
        int expectedBitmapSize = imageSize * AvatarGenerator.SUPERSAMPLING_FACTOR;
        assertEquals(expectedBitmapSize, bitmap.getWidth());
        assertEquals(expectedBitmapSize, bitmap.getHeight());
        // The intrinsic drawable dimensions remain 1x (imageSize).
        assertEquals(imageSize, drawable.getIntrinsicWidth());
        assertEquals(imageSize, drawable.getIntrinsicHeight());
    }
}
