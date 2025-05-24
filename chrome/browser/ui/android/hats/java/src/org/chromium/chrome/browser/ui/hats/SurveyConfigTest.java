// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Unit test for survey config creation. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseJUnit4ClassRunner.class)
public class SurveyConfigTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        SurveyConfig.clearAll();
    }

    @SmallTest
    @Test
    public void readDemoConfig() {
        SurveyConfig config = SurveyConfig.get(mProfile, "testing");
        Assert.assertNotNull("Config is null.", config);
        Assert.assertEquals("Probability is different.", 1.0f, config.mProbability, 0.01f);
        Assert.assertArrayEquals(
                "PSD bit fields is different.",
                config.mPsdBitDataFields,
                new String[] {"Test Field 1", "Test Field 2"});
        Assert.assertArrayEquals(
                "PSD bit fields is different.",
                config.mPsdStringDataFields,
                new String[] {"Test Field 3"});
    }
}
