// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI;
import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI_TIMESTAMP;

import android.content.SharedPreferences;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.base.Clipboard.ImageFileProvider.ClipboardFileMetadata;

/** Tests for ClipboardImageFileProvider. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ClipboardImageFileProviderUnitTest {
    private static final Uri CONTENT_URI = Uri.parse("content://package/path/image.png");

    ClipboardImageFileProvider mClipboardImageFileProvider;

    @Before
    public void setUp() throws Exception {
        mClipboardImageFileProvider = new ClipboardImageFileProvider();
    }

    @After
    public void tearDown() {
        mClipboardImageFileProvider.clearLastCopiedImageMetadata();
    }

    @Test
    @SmallTest
    @Feature("ClipboardImageFileProvider")
    public void testStoreLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String uriString = prefs.getString(CLIPBOARD_SHARED_URI, null);
        long timestampActual = prefs.getLong(CLIPBOARD_SHARED_URI_TIMESTAMP, 0L);
        Assert.assertEquals(CONTENT_URI.toString(), uriString);
        Assert.assertEquals(timestamp, timestampActual);
    }

    @Test
    @SmallTest
    @Feature("ClipboardImageFileProvider")
    public void testGetLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        ClipboardFileMetadata metadata = mClipboardImageFileProvider.getLastCopiedImageMetadata();
        Assert.assertTrue(CONTENT_URI.equals(metadata.uri));
        Assert.assertEquals(timestamp, metadata.timestamp);
    }

    @Test
    @SmallTest
    @Feature("ClipboardImageFileProvider")
    public void testClearLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Assert.assertTrue(prefs.contains(CLIPBOARD_SHARED_URI));
        Assert.assertTrue(prefs.contains(CLIPBOARD_SHARED_URI_TIMESTAMP));

        mClipboardImageFileProvider.clearLastCopiedImageMetadata();
        Assert.assertFalse(prefs.contains(CLIPBOARD_SHARED_URI));
        Assert.assertFalse(prefs.contains(CLIPBOARD_SHARED_URI_TIMESTAMP));
    }
}
