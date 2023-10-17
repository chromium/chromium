// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.Spy;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.util.HashUtil;

/** Unit tests for {@link SettingsSecureBasedIdentificationGenerator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(UniqueIdentificationGeneratorFactoryTest.IDENTITY_GENERATOR_BATCH_NAME)
public class SettingsSecureBasedIdentificationGeneratorTest {
    // Tell R8 this class is spied on and shouldn't be made final.
    @Spy SettingsSecureBasedIdentificationGenerator mGenerator;

    @Before
    public void setUp() {
        mGenerator = Mockito.spy(new SettingsSecureBasedIdentificationGenerator());
    }

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
        doReturn(androidId).when(mGenerator).getAndroidId();

        // Get a unique ID and ensure it is as expected.
        String result = mGenerator.getUniqueId(salt);
        Assert.assertEquals(expectedUniqueId, result);
    }
}
