// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.regex.Pattern;

/** Unit test for UrlAllowlistTest. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlAllowlistTest {
    @Test
    public void testAllowedUrl() throws Throwable {
        HashSet<String> allowedUrls = new HashSet<String>();
        allowedUrls.add("www.chromium.org");
        allowedUrls.add("www.abc.org");
        UrlAllowlist urlAllowlist = new UrlAllowlist(allowedUrls, null);
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.chromium.org:333/home"}));
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.abc.org"}));
        assertFalse(urlAllowlist.isAllowed(new String[] {"http://chromium.org"}));
        // Test any url is allowed.
        assertTrue(
                urlAllowlist.isAllowed(
                        new String[] {"http://www.chromium.org", "http://chromium.org"}));
    }

    @Test
    public void testAllowedRegularExpress() throws Throwable {
        ArrayList<Pattern> allowedRe = new ArrayList<Pattern>();
        allowedRe.add(Pattern.compile(".*chromium.org"));
        allowedRe.add(Pattern.compile(".*abc.org"));
        UrlAllowlist urlAllowlist = new UrlAllowlist(null, allowedRe);
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.chromium.org:333/home"}));
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://chromium.org"}));
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.abc.org"}));
        assertFalse(urlAllowlist.isAllowed(new String[] {"http://abcd.org"}));
        // Test any url is allowed.
        assertTrue(
                urlAllowlist.isAllowed(
                        new String[] {
                            "http://www.chromium.org", "http://chromium.org", "http://abcd.org"
                        }));
    }

    @Test
    public void testEitherUrlOrRegularExpress() throws Throwable {
        HashSet<String> allowedUrls = new HashSet<String>();
        allowedUrls.add("www.chromium.org");
        ArrayList<Pattern> allowedRe = new ArrayList<Pattern>();
        allowedRe.add(Pattern.compile(".*abc.org"));
        UrlAllowlist urlAllowlist = new UrlAllowlist(allowedUrls, allowedRe);
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.chromium.org:333/home"}));
        assertTrue(urlAllowlist.isAllowed(new String[] {"http://www.abc.org"}));
    }

    @Test
    public void testMalformedUrlDisallowed() throws Throwable {
        HashSet<String> allowedUrls = new HashSet<String>();
        allowedUrls.add("www.chromium.org");
        ArrayList<Pattern> allowedRe = new ArrayList<Pattern>();
        allowedRe.add(Pattern.compile(".*abc.org"));
        UrlAllowlist urlAllowlist = new UrlAllowlist(allowedUrls, allowedRe);
        // The url is coming from WebContentObserver.ReadyToCommitNavigation() in production,
        // shouldn't be malformed. Just test it in case.
        assertFalse(urlAllowlist.isAllowed(new String[] {"www.chromium.org:333/home"}));
    }
}
