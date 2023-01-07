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
    private WebsiteEntry getEntryWithTitle(List<WebsiteEntry> entries, String title) {
        for (WebsiteEntry entry : entries) {
            if (entry.getTitleForPreferenceRow().equals(title)) return entry;
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
    public void testLocalhost() {
        Website origin = new Website(WebsiteAddress.create("localhost"), null);
        WebsiteGroup group = new WebsiteGroup(
                origin.getAddress().getDomainAndRegistry(), new ArrayList<>(Arrays.asList(origin)));
        Assert.assertEquals("localhost", group.getDomainAndRegistry());
        Assert.assertEquals("localhost", group.getTitleForPreferenceRow());
        Assert.assertEquals(new GURL("https://localhost"), group.getFaviconUrl());
        Assert.assertTrue(group.matches("local"));
        Assert.assertFalse(group.matches("goog"));
    }

    @Test
    @SmallTest
    public void testIPAddress() {
        Website origin = new Website(WebsiteAddress.create("https://1.1.1.1"), null);
        WebsiteGroup group = new WebsiteGroup(
                origin.getAddress().getDomainAndRegistry(), new ArrayList<>(Arrays.asList(origin)));
        Assert.assertEquals("1.1.1.1", group.getDomainAndRegistry());
        Assert.assertEquals("1.1.1.1", group.getTitleForPreferenceRow());
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
        Assert.assertEquals("google.com", group.getDomainAndRegistry());
        Assert.assertEquals("google.com", group.getTitleForPreferenceRow());
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
        List<WebsiteEntry> entries = WebsiteGroup.groupWebsites(websites);
        Assert.assertEquals(4, entries.size());

        WebsiteEntry google = getEntryWithTitle(entries, "google.com");
        Assert.assertNotNull(google);
        Assert.assertTrue(google instanceof WebsiteGroup);
        Assert.assertEquals(2, ((WebsiteGroup) google).getWebsites().size());

        WebsiteEntry test = getEntryWithTitle(entries, "test.com");
        Assert.assertNotNull(test);
        Assert.assertTrue(test instanceof Website);

        WebsiteEntry localhost = getEntryWithTitle(entries, "localhost");
        Assert.assertNotNull(localhost);
        Assert.assertTrue(localhost instanceof Website);

        WebsiteEntry ipaddr = getEntryWithTitle(entries, "1.1.1.1");
        Assert.assertNotNull(ipaddr);
        Assert.assertTrue(ipaddr instanceof Website);
    }

    @Test
    @SmallTest
    public void testFPSInfo() {
        var fpsInfo = new FPSCookieInfo("google.com", 5);
        Website origin1 = new Website(WebsiteAddress.create("maps.google.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("mail.google.com"), null);
        Website origin3 = new Website(WebsiteAddress.create("docs.google.com"), null);
        origin2.setFPSCookieInfo(fpsInfo);
        WebsiteGroup group = new WebsiteGroup(origin2.getAddress().getDomainAndRegistry(),
                new ArrayList<>(Arrays.asList(origin1, origin2, origin3)));

        Assert.assertEquals(fpsInfo, group.getFPSInfo());
    }
}
