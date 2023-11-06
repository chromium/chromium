// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link Origin}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OriginTest {
    @Test
    @SmallTest
    public void testTransformation() {
        Assert.assertEquals(Uri.parse("http://example.com:123/").getPort(), 123);

        // Unlike origin.cc, the returned Uri has a port of -1 if it is the default port for the
        // scheme.
        check("http://192.168.9.1/", "http", "192.168.9.1", -1);

        // Test cases for origin.cc that do *not* work with Origin.java:

        // check("http://[2001:db8::1]/", "http", "[2001:db8::1]", 80);
        // This is because Uri cannot deal with IPv6 URLS, eg:
        // Uri.parse("http://[2001:db8::1]/").getHost() returns "[2001"

        // check("http://☃.net/", "http", "xn--n3h.net", 80);
        // check("blob:http://☃.net/", "http", "xn--n3h.net", 80);
        // We don't perform punycode substitution so the string is unchanged.

        check("http://example.com/", "http", "example.com", -1);
        check("http://sub.example.com/", "http", "sub.example.com", -1);
        check("http://example.com:123/", "http", "example.com", 123);
        check("https://example.com/", "https", "example.com", -1);
        check("https://example.com:123/", "https", "example.com", 123);
        check("http://user:pass@example.com/", "http", "example.com", -1);
        check("http://example.com:123/?query", "http", "example.com", 123);
        check("https://example.com/#1234", "https", "example.com", -1);
        check("https://u:p@example.com:123/?query#1234", "https", "example.com", 123);
    }

    private static void check(String url, String scheme, String host, int port) {
        Origin origin = Origin.create(url);
        Assert.assertEquals(scheme, origin.uri().getScheme());
        Assert.assertEquals(host, origin.uri().getHost());
        Assert.assertEquals(port, origin.uri().getPort());

        Assert.assertEquals(
                origin.toString(), scheme + "://" + host + (port == -1 ? "" : ":" + port));
    }

    @Test
    @SmallTest
    public void testConstruction() {
        Origin origin = Origin.create("http://www.example.com/path/to/page.html");
        Assert.assertEquals("http://www.example.com", origin.toString());
    }

    @Test
    @SmallTest
    public void testEquality() {
        Origin origin1 = Origin.create("http://www.example.com/page1.html");
        Origin origin2 = Origin.create("http://www.example.com/page2.html");
        Assert.assertEquals(origin1, origin2);
        Assert.assertEquals(origin1.hashCode(), origin2.hashCode());

        // Note http*s*.
        Origin origin3 = Origin.create("https://www.example.com/page3.html");
        Assert.assertNotEquals(origin1, origin3);
    }

    @Test
    @SmallTest
    public void testToUri() {
        Origin origin = Origin.create(Uri.parse("http://www.example.com/page.html"));
        Uri uri = Uri.parse("http://www.example.com");
        Assert.assertEquals(uri, origin.uri());
    }

    @Test
    @SmallTest
    public void testToString() {
        Origin origin = Origin.create("http://www.example.com/page.html");
        Assert.assertEquals("http://www.example.com", origin.toString());
    }

    @Test
    @SmallTest
    public void testValidity() {
        Assert.assertNotNull(Origin.create("http://www.example.com"));
        Assert.assertNull(Origin.create("null"));
        Assert.assertNull(Origin.create("www.example.com"));
    }
}
