// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Unit tests for RoundedIconGenerator.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RoundedIconGeneratorTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
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
        Assert.assertEquals("file:///home/chrome/test.html",
                getIconTextForUrl("file:///home/chrome/test.html", false));
        Assert.assertEquals("data:image", getIconTextForUrl("data:image", false));
    }

    /**
     * Verifies that asking for more letters than can be served does not crash.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "RoundedIconGenerator"})
    public void testGenerateIconForText() {
        final int iconSizeDp = 32;
        final int iconCornerRadiusDp = 20;
        final int iconTextSizeDp = 12;

        int iconColor = Color.GRAY;
        RoundedIconGenerator generator = new RoundedIconGenerator(mContext.getResources(),
                iconSizeDp, iconSizeDp, iconCornerRadiusDp, iconColor, iconTextSizeDp);

        Assert.assertTrue(generator.generateIconForText("") != null);
        Assert.assertTrue(generator.generateIconForText("A") != null);
    }
}
