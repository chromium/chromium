// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Color;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Unit tests for RoundedIconGenerator. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RoundedIconGeneratorTest {
    private static Context sContext;

    @BeforeClass
    public static void setUp() {
        sContext = InstrumentationRegistry.getTargetContext();
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    private String getIconTextForUrl(String url, boolean includePrivateRegistries) {
        return RoundedIconGenerator.getIconTextForUrl(url, includePrivateRegistries);
    }

    /**
     * Verifies that RoundedIconGenerator's ability to generate icons based on URLs considers the
     * appropriate parts of the URL for the icon to generate.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "RoundedIconGenerator"})
    public void testGetIconTextForUrl() {
        // Verify valid domains when including private registries.
        Assert.assertEquals("google.com", getIconTextForUrl("https://google.com/", true));
        Assert.assertEquals("google.com", getIconTextForUrl("https://www.google.com:443/", true));
        Assert.assertEquals("google.com", getIconTextForUrl("https://mail.google.com/", true));
        Assert.assertEquals("foo.appspot.com", getIconTextForUrl("https://foo.appspot.com/", true));

        // Verify valid domains when not including private registries.
        Assert.assertEquals("appspot.com", getIconTextForUrl("https://foo.appspot.com/", false));

        // Verify Chrome-internal
        Assert.assertEquals("chrome", getIconTextForUrl("chrome://about", false));
        Assert.assertEquals("chrome", getIconTextForUrl("chrome-native://newtab", false));

        // Verify that other URIs from which a hostname can be resolved use that.
        Assert.assertEquals("localhost", getIconTextForUrl("http://localhost/", false));
        Assert.assertEquals("google-chrome", getIconTextForUrl("https://google-chrome/", false));
        Assert.assertEquals("127.0.0.1", getIconTextForUrl("http://127.0.0.1/", false));

        // Verify that the fallback is the the URL itself.
        Assert.assertEquals(
                "file:///home/chrome/test.html",
                getIconTextForUrl("file:///home/chrome/test.html", false));
        Assert.assertEquals("data:image", getIconTextForUrl("data:image", false));
    }

    /** Verifies that asking for more letters than can be served does not crash. */
    @Test
    @SmallTest
    @Feature({"Browser", "RoundedIconGenerator"})
    public void testGenerateIconForText() {
        final int iconSizeDp = 32;
        final int iconCornerRadiusDp = 20;
        final int iconTextSizeDp = 12;

        int iconColor = Color.GRAY;
        RoundedIconGenerator generator =
                new RoundedIconGenerator(
                        sContext.getResources(),
                        iconSizeDp,
                        iconSizeDp,
                        iconCornerRadiusDp,
                        iconColor,
                        iconTextSizeDp);

        Assert.assertTrue(generator.generateIconForText("") != null);
        Assert.assertTrue(generator.generateIconForText("A") != null);
    }
}
