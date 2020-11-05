// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.util.HashUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Unit tests for {@link SettingsSecureBasedIdentificationGenerator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(UniqueIdentificationGeneratorFactoryTest.IDENTITY_GENERATOR_BATCH_NAME)
public class SettingsSecureBasedIdentificationGeneratorTest {
    private static final String FLAG_ANDROID_ID = "android_id";

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha"})
    public void testAndroidIdSuccessWithSalt() {
        String androidId = "42";
        String salt = "mySalt";
        String expected = HashUtil.getMd5Hash(new HashUtil.Params(androidId).withSalt(salt));
        runTest(androidId, salt, expected);
    }

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha"})
    public void testAndroidIdSuccessWithoutSalt() {
        String androidId = "42";
        String expected = HashUtil.getMd5Hash(new HashUtil.Params(androidId));
        runTest(androidId, null, expected);
    }

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha"})
    public void testAndroidIdFailureWithSalt() {
        String androidId = null;
        String salt = "mySalt";
        String expected = "";
        runTest(androidId, salt, expected);
    }

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha"})
    public void testAndroidIdFailureWithoutSalt() {
        String androidId = null;
        String salt = null;
        String expected = "";
        runTest(androidId, salt, expected);
    }

    private void runTest(String androidId, String salt, String expectedUniqueId) {
        AdvancedMockContext context = new AdvancedMockContext();
        TestGenerator generator = new TestGenerator(context, androidId);

        // Get a unique ID and ensure it is as expected.
        String result = generator.getUniqueId(salt);
        Assert.assertEquals(expectedUniqueId, result);
    }

    private static class TestGenerator extends SettingsSecureBasedIdentificationGenerator {
        private final AdvancedMockContext mContext;
        private final String mAndroidId;

        TestGenerator(AdvancedMockContext context, String androidId) {
            super(context);
            mContext = context;
            mAndroidId = androidId;
        }

        @Override
        String getAndroidId() {
            mContext.setFlag(FLAG_ANDROID_ID);
            return mAndroidId;
        }
    }
}
