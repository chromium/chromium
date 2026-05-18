// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.webshare;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.mojom.Url;
import org.chromium.webshare.mojom.ShareError;
import org.chromium.webshare.mojom.ShareService;

/** Unit tests for {@link ShareServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShareServiceImplTest {
    // Verifies all the file names that are not allowed to be shared on Android.
    @Test
    @SmallTest
    public void testExtensionFormattingDisallowed() {
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo/bar.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo\\bar\u03C0.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo\\bar.tx\u03C0t"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("https://example.com/a/b.html"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo/bar.txt/"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foobar.tx\\t"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("hello"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("hellotxt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("https://example.com/a/.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("/.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".."));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("bar.tx\u03C0t"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("."));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(" my_name "));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(" . "));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".config"));
    }

    // Verifies all the file names that are allowed to be shared on Android.
    @Test
    @SmallTest
    public void testExtensionFormattingAllowed() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename(".hello.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("bar.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("bar\u03C0.txt"));
    }

    @Test
    @SmallTest
    public void testExecutable() {
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.apk"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.dex"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.sh"));
    }

    @Test
    @SmallTest
    public void testContent() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("diagram.svg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("greeting.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("movie.mpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("photo.jpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("recording.wav"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("statistics.csv"));
    }

    @Test
    @SmallTest
    public void testCompound() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("powerless.sh.txt"));
    }

    @Test
    @SmallTest
    public void testUnsupportedMime() {
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("application/x-shockwave-flash"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("image/wmf"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("text/calendar"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("video/H264"));
    }

    @Test
    @SmallTest
    public void testSupportedMime() {
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("application/pdf"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("audio/mp3"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("audio/mpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("audio/wav"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("image/avif"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("image/jpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("image/svg+xml"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("text/csv"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("text/plain"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("video/mpeg"));
    }

    @Test
    @SmallTest
    public void testInvalidScheme() {
        // Using 1-element arrays to allow anonymous inner classes (WebShareDelegate and
        // Share_Response) to modify local state, as captured variables must be effectively final.
        int[] badMessageReason = new int[1];
        int[] shareError = new int[1];

        ShareServiceImpl.WebShareDelegate mockDelegate =
                new ShareServiceImpl.WebShareDelegate() {
                    @Override
                    public boolean canShare() {
                        return true;
                    }

                    @Override
                    public void share(ShareParams params) {}

                    @Override
                    public WindowAndroid getWindowAndroid() {
                        return null;
                    }

                    @Override
                    public void terminateRendererDueToBadMessage(int reason) {
                        badMessageReason[0] = reason;
                    }
                };

        ShareServiceImpl shareService = new ShareServiceImpl(mockDelegate);
        Url url = new Url();
        url.url = "javascript:alert(1)";

        shareService.share(
                "title",
                "text",
                url,
                null,
                new ShareService.Share_Response() {
                    @Override
                    public void call(int error) {
                        shareError[0] = error;
                    }
                });

        Assert.assertEquals(ShareError.PERMISSION_DENIED, shareError[0]);
        Assert.assertEquals(11 /* RFH_INVALID_WEB_FRAME_URL */, badMessageReason[0]);
    }
}
