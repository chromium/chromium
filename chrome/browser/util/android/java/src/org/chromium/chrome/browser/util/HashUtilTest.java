// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

@RunWith(ChromeJUnit4ClassRunner.class)
public class HashUtilTest {
    @Test
    @SmallTest
    @Feature({"Sync", "Omaha"})
    public void testMd5HashGivesCorrectString() {
        Assert.assertEquals("8e8cd7e8797678284984aa304e779ba5",
                HashUtil.getMd5Hash(new HashUtil.Params("Chrome for Android")));
        // WARNING: The expected value for this must NEVER EVER change. Ever.
        // See http://crbug.com/179565.
        Assert.assertEquals("6aa987da27016dade54b24ff5b846111",
                HashUtil.getMd5Hash(new HashUtil.Params("Chrome for Android").withSalt("mySalt")));
    }
}
