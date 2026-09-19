// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ExtensionUrlUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionUrlUtilUnitTest {
    private static final String VALID_EXTENSION_ID = "abcdefghijklmnopqrstuvwxyzabcdef";
    private static final String VALID_ORIGIN =
            UrlConstants.CHROME_EXTENSION_SCHEME + "://" + VALID_EXTENSION_ID;

    @Test
    public void testGetOrigin_ValidUrl() {
        Assert.assertEquals(VALID_ORIGIN, ExtensionUrlUtil.getOrigin(VALID_ORIGIN));
    }

    @Test
    public void testGetOrigin_InvalidScheme() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> ExtensionUrlUtil.getOrigin("https://example.com"));
    }

    @Test
    public void testGetOrigin_NoHost() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> ExtensionUrlUtil.getOrigin(UrlConstants.CHROME_EXTENSION_SCHEME + ":///"));
    }

    @Test
    public void testGetOrigin_NullUrl() {
        Assert.assertThrows(
                IllegalArgumentException.class, () -> ExtensionUrlUtil.getOrigin((String) null));
    }

    @Test
    public void testGetOrigin_EmptyUrl() {
        Assert.assertThrows(IllegalArgumentException.class, () -> ExtensionUrlUtil.getOrigin(""));
    }

    @Test
    public void testIsExtensionUrl_NullString() {
        Assert.assertFalse(ExtensionUrlUtil.isExtensionUrl(null));
    }

    @Test
    public void testIsExtensionUrl_EmptyString() {
        Assert.assertFalse(ExtensionUrlUtil.isExtensionUrl(""));
    }

    @Test
    public void testIsExtensionUrl_ValidExtensionUrl() {
        Assert.assertTrue(ExtensionUrlUtil.isExtensionUrl(VALID_ORIGIN));
        Assert.assertTrue(ExtensionUrlUtil.isExtensionUrl(VALID_ORIGIN + "/"));
    }

    @Test
    public void testIsExtensionUrl_OtherUrl() {
        Assert.assertFalse(ExtensionUrlUtil.isExtensionUrl("https://example.com"));
    }
}
