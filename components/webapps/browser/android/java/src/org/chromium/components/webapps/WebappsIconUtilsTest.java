// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import static org.junit.Assert.assertNotNull;

import android.graphics.Bitmap;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.url.GURL;

/** Tests WebappsIconUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.Q)
public class WebappsIconUtilsTest {

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testGenerateHomeScreenIcon_NullUrl() {
        GURL url = GURL.emptyGURL();
        Bitmap icon = WebappsIconUtils.generateHomeScreenIcon(url, 0, 0, 0);
        // Should return a fallback icon (not null)
        assertNotNull(icon);
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testGenerateHomeScreenIcon_ValidUrl() {
        GURL url = new GURL("https://example.com");
        Bitmap icon = WebappsIconUtils.generateHomeScreenIcon(url, 0, 0, 0);
        assertNotNull(icon);
    }
}
