// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.url_formatter;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Unit tests for {@link UrlFormatter}.
 *
 * These tests are basic sanity checks to ensure the plumbing is working correctly. The wrapped
 * functions are tested much more thoroughly elsewhere.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class UrlFormatterUnitTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testFixupUrl() {
        Assert.assertEquals("http://google.com/", UrlFormatter.fixupUrl("google.com").getSpec());
        Assert.assertEquals("chrome://version/", UrlFormatter.fixupUrl("about:").getSpec());
        Assert.assertEquals("file:///mail.google.com:/",
                UrlFormatter.fixupUrl("//mail.google.com:/").getSpec());
        Assert.assertFalse(UrlFormatter.fixupUrl("0x100.0").isValid());
    }

    @Test
    @SmallTest
    public void testFormatUrlForDisplayOmitUsernamePassword() {
        Assert.assertEquals("http://google.com/path",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword("http://google.com/path"));
        Assert.assertEquals("http://google.com",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword(
                        "http://user:pass@google.com"));
        Assert.assertEquals("http://google.com",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword("http://user@google.com"));
    }
}
