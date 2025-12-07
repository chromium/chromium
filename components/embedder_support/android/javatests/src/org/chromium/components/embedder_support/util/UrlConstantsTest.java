// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.url.GURL;
import org.chromium.url.GURL.BadSerializerVersionException;

/** Unit tests for {@link UrlConstants}. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests pre-native startup behaviour and thus can't be batched.")
public class UrlConstantsTest {
    @Test
    @SmallTest
    public void testNtpUrl() {
        GURL ntpGurl;
        try {
            ntpGurl = UrlConstants.ntpGurl();
        } catch (BadSerializerVersionException be) {
            Assert.assertFalse(
                    "Serialized value for ntpGurl in UrlConstants has a stale serializer version",
                    true);
            return;
        }

        Assert.assertTrue(ntpGurl.isValid());
        Assert.assertEquals(UrlConstants.NTP_HOST, ntpGurl.getHost());
        Assert.assertEquals(UrlConstants.CHROME_SCHEME, ntpGurl.getScheme());
        Assert.assertTrue(UrlUtilities.isNtpUrl(UrlConstants.ntpGurl()));
    }
}
