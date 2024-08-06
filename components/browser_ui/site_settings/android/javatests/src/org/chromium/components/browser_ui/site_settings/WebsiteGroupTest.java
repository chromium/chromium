// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for WebsiteGroup. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebsiteGroupTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    @Mock private BrowserContextHandle mContextHandleMock;

    private WebsiteEntry getEntryWithTitle(List<WebsiteEntry> entries, String title) {
        for (WebsiteEntry entry : entries) {
            if (entry.getTitleForPreferenceRow().equals(title)) return entry;
        }
        return null;
    }

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setupTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mBridgeMock);
    }

    @Test
    @SmallTest
    public void testLocalhost() {
        Website origin = new Website(WebsiteAddress.create("localhost"), null);
        WebsiteGroup group =
                new WebsiteGroup(origin.getAddress().getDomainAndRegistry(), Arrays.asList(origin));
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
        WebsiteGroup group =
                new WebsiteGroup(origin.getAddress().getDomainAndRegistry(), Arrays.asList(origin));
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
        WebsiteGroup group =
                new WebsiteGroup(
                        origin1.getAddress().getDomainAndRegistry(),
                        Arrays.asList(origin1, origin2));
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
        List<Website> websites =
                Arrays.asList(
                        new Website(WebsiteAddress.create("https://one.google.com"), null),
                        new Website(WebsiteAddress.create("http://two.google.com"), null),
                        new Website(WebsiteAddress.create("https://test.com"), null),
                        new Website(WebsiteAddress.create("localhost"), null),
                        new Website(WebsiteAddress.create("1.1.1.1"), null));
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
    public void testRWSInfo() {
        var rwsInfo =
                new RWSCookieInfo(
                        "google.com",
                        Arrays.asList(
                                new Website(null, null),
                                new Website(null, null),
                                new Website(null, null),
                                new Website(null, null),
                                new Website(null, null)));
        Website origin1 = new Website(WebsiteAddress.create("maps.google.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("mail.google.com"), null);
        Website origin3 = new Website(WebsiteAddress.create("docs.google.com"), null);
        origin2.setRWSCookieInfo(rwsInfo);
        WebsiteGroup group =
                new WebsiteGroup(
                        origin2.getAddress().getDomainAndRegistry(),
                        Arrays.asList(origin1, origin2, origin3));

        Assert.assertEquals(rwsInfo, group.getRWSInfo());
    }

    @Test
    @SmallTest
    public void testCookieDeletionDisabled() {
        String origin1 = "https://google.com";
        String origin2 = "http://maps.google.com";
        Website site1 = new Website(WebsiteAddress.create(origin1), null);
        Website site2 = new Website(WebsiteAddress.create(origin2), null);
        when(mBridgeMock.isCookieDeletionDisabled(eq(mContextHandleMock), eq(origin1)))
                .thenReturn(false);
        when(mBridgeMock.isCookieDeletionDisabled(eq(mContextHandleMock), eq(origin2)))
                .thenReturn(true);
        // Individual sites should just return the same thing the bridge does.
        Assert.assertFalse(site1.isCookieDeletionDisabled(mContextHandleMock));
        Assert.assertTrue(site2.isCookieDeletionDisabled(mContextHandleMock));
        // This group consists entirely of sites with cookie deletion being possible.
        WebsiteGroup group1 =
                new WebsiteGroup(site1.getAddress().getDomainAndRegistry(), Arrays.asList(site1));
        Assert.assertFalse(group1.isCookieDeletionDisabled(mContextHandleMock));
        // This group consists entirely of sites with cookie deletion disabled.
        WebsiteGroup group2 =
                new WebsiteGroup(site2.getAddress().getDomainAndRegistry(), Arrays.asList(site2));
        Assert.assertTrue(group2.isCookieDeletionDisabled(mContextHandleMock));
        // This one has at least one, for which deletion is possible.
        WebsiteGroup group3 =
                new WebsiteGroup(
                        site1.getAddress().getDomainAndRegistry(), Arrays.asList(site1, site2));
        Assert.assertFalse(group3.isCookieDeletionDisabled(mContextHandleMock));
    }

    @Test
    @SmallTest
    public void testHasInstalledApp() {
        String origin1 = "https://google.com";
        String origin2 = "http://maps.google.com";
        Website site1 = new Website(WebsiteAddress.create(origin1), null);
        Website site2 = new Website(WebsiteAddress.create(origin2), null);
        WebsiteGroup group =
                new WebsiteGroup(
                        site1.getAddress().getDomainAndRegistry(), Arrays.asList(site1, site2));

        Set<String> apps1 = new HashSet<>(Arrays.asList("http://example.com"));
        Assert.assertFalse(group.hasInstalledApp(apps1));

        Set<String> apps2 =
                new HashSet<>(Arrays.asList("http://example.com", "https://google.com"));
        Assert.assertTrue(group.hasInstalledApp(apps2));
    }
}
