// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.not;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/** Tests for WebsiteAddress. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebsiteAddressTest {
    @BeforeClass
    public static void setUp() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "Main"})
    public void testCreate() {
        Assert.assertEquals(null, WebsiteAddress.create(null));
        Assert.assertEquals(null, WebsiteAddress.create(""));

        WebsiteAddress httpAddress = WebsiteAddress.create("http://a.google.com");
        Assert.assertEquals("http://a.google.com", httpAddress.getOrigin());
        Assert.assertEquals("a.google.com", httpAddress.getHost());
        Assert.assertEquals("a.google.com", httpAddress.getTitle());
        Assert.assertFalse(httpAddress.getIsAnySubdomainPattern());

        WebsiteAddress http8080Address = WebsiteAddress.create("http://a.google.com:8080/");
        Assert.assertEquals("http://a.google.com:8080", http8080Address.getOrigin());
        Assert.assertEquals("a.google.com", http8080Address.getHost());
        Assert.assertEquals("http://a.google.com:8080", http8080Address.getTitle());
        Assert.assertFalse(http8080Address.getIsAnySubdomainPattern());

        WebsiteAddress httpsAddress = WebsiteAddress.create("https://a.google.com/");
        Assert.assertEquals("https://a.google.com", httpsAddress.getOrigin());
        Assert.assertEquals("a.google.com", httpsAddress.getHost());
        Assert.assertEquals("https://a.google.com", httpsAddress.getTitle());
        Assert.assertFalse(httpsAddress.getIsAnySubdomainPattern());

        WebsiteAddress hostAddress = WebsiteAddress.create("a.google.com");
        Assert.assertEquals("http://a.google.com", hostAddress.getOrigin());
        Assert.assertEquals("a.google.com", hostAddress.getHost());
        Assert.assertEquals("a.google.com", hostAddress.getTitle());
        Assert.assertFalse(hostAddress.getIsAnySubdomainPattern());

        WebsiteAddress anySubdomainAddress = WebsiteAddress.create("[*.]google.com");
        Assert.assertEquals("http://google.com", anySubdomainAddress.getOrigin());
        Assert.assertEquals("google.com", anySubdomainAddress.getHost());
        Assert.assertEquals("google.com", anySubdomainAddress.getTitle());
        Assert.assertTrue(anySubdomainAddress.getIsAnySubdomainPattern());

        WebsiteAddress schemefulSitePattern = WebsiteAddress.create("https://[*.]google.com");
        Assert.assertEquals("https://google.com", schemefulSitePattern.getOrigin());
        Assert.assertEquals("google.com", schemefulSitePattern.getHost());
        Assert.assertEquals("https://google.com", schemefulSitePattern.getTitle());
        Assert.assertFalse(schemefulSitePattern.getIsAnySubdomainPattern());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testEqualsHashCodeCompareTo() {
        Object[][] testData = {
            {0, "http://google.com", "http://google.com"},
            {-1, "[*.]google.com", "http://google.com"},
            {-1, "[*.]google.com", "http://a.google.com"},
            {-1, "[*.]a.com", "[*.]b.com"},
            {0, "[*.]google.com", "google.com"},
            {-1, "[*.]google.com", "a.google.com"},
            {-1, "http://google.com", "http://a.google.com"},
            {-1, "http://a.google.com", "http://a.a.google.com"},
            {-1, "http://a.a.google.com", "http://a.b.google.com"},
            {1, "http://a.b.google.com", "http://google.com"},
            {-1, "http://google.com", "https://google.com"},
            {-1, "http://google.com", "https://a.google.com"},
            {1, "https://b.google.com", "https://a.google.com"},
            {-1, "http://a.com", "http://b.com"},
            {-1, "http://a.com", "http://a.b.com"}
        };

        for (int i = 0; i < testData.length; ++i) {
            Object[] testRow = testData[i];

            int compareToResult = (Integer) testRow[0];

            String string1 = (String) testRow[1];
            String string2 = (String) testRow[2];

            WebsiteAddress addr1 = WebsiteAddress.create(string1);
            WebsiteAddress addr2 = WebsiteAddress.create(string2);

            Assert.assertEquals(
                    "\"" + string1 + "\" vs \"" + string2 + "\"",
                    compareToResult,
                    Integer.signum(addr1.compareTo(addr2)));

            // Test that swapping arguments gives an opposite result.
            Assert.assertEquals(
                    "\"" + string2 + "\" vs \"" + string1 + "\"",
                    -compareToResult,
                    Integer.signum(addr2.compareTo(addr1)));

            if (compareToResult == 0) {
                Assert.assertTrue(addr1.equals(addr2));
                Assert.assertTrue(addr2.equals(addr1));
                Assert.assertEquals(addr1.hashCode(), addr2.hashCode());
            } else {
                Assert.assertFalse(addr1.equals(addr2));
                Assert.assertFalse(addr2.equals(addr1));
                // Note: hash codes could still be the same.
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testConvertHostToOrigin() {
        WebsiteAddress first = WebsiteAddress.create("developer.android.com");
        WebsiteAddress second = WebsiteAddress.create("http://developer.android.com");
        Assert.assertThat(first, not(equalTo(second)));

        WebsiteAddress converted = WebsiteAddress.create(first.getOrigin());
        Assert.assertEquals(converted, second);
    }
}
