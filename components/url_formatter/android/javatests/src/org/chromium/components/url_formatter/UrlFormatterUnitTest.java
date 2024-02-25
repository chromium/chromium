// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.url_formatter;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;
import org.chromium.url.GURLJavaTestHelper;

import java.util.function.Function;

/**
 * Unit tests for {@link UrlFormatter}.
 *
 * <p>These tests are basic checks to ensure the plumbing is working correctly. The wrapped
 * functions are tested much more thoroughly elsewhere.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class UrlFormatterUnitTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        GURLJavaTestHelper.nativeInitializeICU();
    }

    @Test
    @SmallTest
    public void testFixupUrl() {
        assertEquals("http://google.com/", UrlFormatter.fixupUrl("google.com").getSpec());
        assertEquals("chrome://version/", UrlFormatter.fixupUrl("about:").getSpec());
        assertEquals(
                "file:///mail.google.com:/",
                UrlFormatter.fixupUrl("//mail.google.com:/").getSpec());
        Assert.assertFalse(UrlFormatter.fixupUrl("0x100.0").isValid());
    }

    @Test
    @SmallTest
    public void testFormatUrlForDisplayOmitUsernamePassword() {
        assertEquals(
                "http://google.com/path",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword("http://google.com/path"));
        assertEquals(
                "http://google.com",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword(
                        "http://user:pass@google.com"));
        assertEquals(
                "http://google.com",
                UrlFormatter.formatUrlForDisplayOmitUsernamePassword("http://user@google.com"));
    }

    @Test
    @SmallTest
    public void testFormatUrlForDisplayOmitSchemeOmitTrivialSubdomains() {
        Function<String, String> f =
                UrlFormatter::formatUrlForDisplayOmitSchemeOmitTrivialSubdomains;

        assertEquals("google.com/path", f.apply("http://user:pass@google.com/path"));
        assertEquals("chrome://version", f.apply("chrome://version"));
        assertEquals("äpple.de", f.apply("https://äpple.de"));
        assertEquals("xn--pple-koa.com", f.apply("https://äpple.com"));
        assertEquals("مثال.إختبار", f.apply("https://xn--mgbh0fb.xn--kgbechtv/"));
        assertEquals("example.com/ test", f.apply("http://user:password@example.com/%20test"));
    }

    @Test
    @SmallTest
    public void testFormatUrlForDisplayOmitSchemePathAndTrivialSubdomains() {
        Function<GURL, String> f =
                UrlFormatter::formatUrlForDisplayOmitSchemePathAndTrivialSubdomains;

        assertEquals("google.com", f.apply(new GURL("http://user:pass@google.com/path")));
        assertEquals("chrome://version", f.apply(new GURL("chrome://version")));
        assertEquals("äpple.de", f.apply(new GURL("https://äpple.de")));
        assertEquals("xn--pple-koa.com", f.apply(new GURL("https://äpple.com")));
        assertEquals("مثال.إختبار", f.apply(new GURL("https://xn--mgbh0fb.xn--kgbechtv/")));
    }
}
