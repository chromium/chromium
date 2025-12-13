// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.content.Intent;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link PlatformUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlatformUtilTest {
    private ShadowApplication mShadowApplication;
    private static final String EXTENSION_CONTENT_URI_STRING =
            "content://com.android.extenalstorage.documents/tree/primary%3Atest-extension";

    @Before
    public void setUp() {
        mShadowApplication = shadowOf((Application) ApplicationProvider.getApplicationContext());
    }

    @Test
    @SmallTest
    public void testShowItemInFolder() {
        PlatformUtil.showItemInFolder(EXTENSION_CONTENT_URI_STRING);

        Intent intent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(intent);
        Assert.assertEquals(Intent.ACTION_VIEW, intent.getAction());
        Assert.assertEquals(Uri.parse(EXTENSION_CONTENT_URI_STRING), intent.getData());
        Assert.assertEquals("*/*", intent.getType());
        Assert.assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
    }
}
