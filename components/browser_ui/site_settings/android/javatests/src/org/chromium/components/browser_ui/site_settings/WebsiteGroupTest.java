// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for WebsiteGroup.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebsiteGroupTest {
    private WebsiteGroup getGroupWithTitle(List<WebsiteGroup> groups, String title) {
        for (WebsiteGroup group : groups) {
            if (group.getTitle().equals(title)) return group;
        }
        return null;
    }

    @BeforeClass
    public static void setUp() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Test
    @SmallTest
    public void testOneOrigin() {
        Website origin = new Website(WebsiteAddress.create("http://test.google.com"), null);
        WebsiteGroup group = new WebsiteGroup(
                origin.getAddress().getDomainAndRegistry(), new ArrayList<>(Arrays.asList(origin)));
        Assert.assertTrue(group.hasOneOrigin());
        Assert.assertEquals("google.com", group.getDomainAndRegistry());
        Assert.assertEquals("test.google.com", group.getTitle());
        Assert.assertEquals(new GURL("http://test.google.com"), group.getFaviconUrl());
        Assert.assertTrue(group.matches("goog"));
        Assert.assertFalse(group.matches("face"));
    }

    @Test
    @SmallTest
    public void testLocalhost() {
        Website origin = new Website(WebsiteAddress.create("localhost"), null);
        WebsiteGroup group = new WebsiteGroup(
                origin.getAddress().getDomainAndRegistry(), new ArrayList<>(Arrays.asList(origin)));
        Assert.assertTrue(group.hasOneOrigin());
        Assert.assertEquals("localhost", group.getDomainAndRegistry());
        Assert.assertEquals("localhost", group.getTitle());
        Assert.assertEquals(new GURL("http://localhost"), group.getFaviconUrl());
        Assert.assertTrue(group.matches("local"));
        Assert.assertFalse(group.matches("goog"));
    }

    @Test
    @SmallTest
    public void testIPAddress() {
        Website origin = new Website(WebsiteAddress.create("https://1.1.1.1"), null);
        WebsiteGroup group = new WebsiteGroup(
                origin.getAddress().getDomainAndRegistry(), new ArrayList<>(Arrays.asList(origin)));
        Assert.assertTrue(group.hasOneOrigin());
        Assert.assertEquals("1.1.1.1", group.getDomainAndRegistry());
        Assert.assertEquals("1.1.1.1", group.getTitle());
        Assert.assertEquals(new GURL("https://1.1.1.1"), group.getFaviconUrl());
        Assert.assertTrue(group.matches("1.1"));
        Assert.assertFalse(group.matches("goog"));
    }

    @Test
    @SmallTest
    public void testMultipleOrigins() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.google.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.google.com"), null);
        WebsiteGroup group = new WebsiteGroup(origin1.getAddress().getDomainAndRegistry(),
                new ArrayList<>(Arrays.asList(origin1, origin2)));
        Assert.assertFalse(group.hasOneOrigin());
        Assert.assertEquals("google.com", group.getDomainAndRegistry());
        Assert.assertEquals("google.com", group.getTitle());
        Assert.assertEquals(new GURL("https://google.com"), group.getFaviconUrl());
        Assert.assertTrue(group.matches("goog"));
        Assert.assertTrue(group.matches("one"));
        Assert.assertTrue(group.matches("two"));
        Assert.assertFalse(group.matches("face"));
    }

    @Test
    @SmallTest
    public void testGroupWebsites() {
        List<Website> websites = new ArrayList<>(
                Arrays.asList(new Website(WebsiteAddress.create("https://one.google.com"), null),
                        new Website(WebsiteAddress.create("http://two.google.com"), null),
                        new Website(WebsiteAddress.create("https://test.com"), null),
                        new Website(WebsiteAddress.create("localhost"), null),
                        new Website(WebsiteAddress.create("1.1.1.1"), null)));
        List<WebsiteGroup> groups = WebsiteGroup.groupWebsites(websites);
        Assert.assertEquals(4, groups.size());

        WebsiteGroup google = getGroupWithTitle(groups, "google.com");
        Assert.assertNotNull(google);
        Assert.assertEquals(2, google.getWebsites().size());

        WebsiteGroup test = getGroupWithTitle(groups, "test.com");
        Assert.assertNotNull(test);
        Assert.assertEquals(1, test.getWebsites().size());

        WebsiteGroup localhost = getGroupWithTitle(groups, "localhost");
        Assert.assertNotNull(localhost);
        Assert.assertEquals(1, localhost.getWebsites().size());

        WebsiteGroup ipaddr = getGroupWithTitle(groups, "1.1.1.1");
        Assert.assertNotNull(ipaddr);
        Assert.assertEquals(1, ipaddr.getWebsites().size());
    }
}
