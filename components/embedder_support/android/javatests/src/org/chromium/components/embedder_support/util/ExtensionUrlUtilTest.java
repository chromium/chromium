// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for {@link ExtensionUrlUtil}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ExtensionUrlUtilTest {
    private static final String VALID_EXTENSION_ID = "abcdefghijklmnopqrstuvwxyzabcdef";
    private static final String VALID_ORIGIN =
            UrlConstants.CHROME_EXTENSION_SCHEME + "://" + VALID_EXTENSION_ID;

    @Test
    @SmallTest
    public void testGetOriginValidUrl() {
        Assert.assertEquals(VALID_ORIGIN, ExtensionUrlUtil.getOrigin(VALID_ORIGIN));
    }

    @Test
    @SmallTest
    public void testGetOriginInvalidScheme() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> ExtensionUrlUtil.getOrigin("https://example.com"));
    }

    @Test
    @SmallTest
    public void testGetOriginNoHost() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> ExtensionUrlUtil.getOrigin(UrlConstants.CHROME_EXTENSION_SCHEME + ":///"));
    }

    @Test
    @SmallTest
    public void testGetOriginNullUrl() {
        Assert.assertThrows(
                IllegalArgumentException.class, () -> ExtensionUrlUtil.getOrigin((String) null));
    }

    @Test
    @SmallTest
    public void testGetOriginEmptyUrl() {
        Assert.assertThrows(IllegalArgumentException.class, () -> ExtensionUrlUtil.getOrigin(""));
    }
}
