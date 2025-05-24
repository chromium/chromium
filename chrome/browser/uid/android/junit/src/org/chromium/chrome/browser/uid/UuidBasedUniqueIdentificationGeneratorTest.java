// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link UuidBasedUniqueIdentificationGenerator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(UniqueIdentificationGeneratorFactoryTest.IDENTITY_GENERATOR_BATCH_NAME)
public class UuidBasedUniqueIdentificationGeneratorTest {
    // Tell R8 this class is spied on and shouldn't be made final.
    @Spy UuidBasedUniqueIdentificationGenerator mGenerator;

    @Before
    public void setUp() {
        ChromeSharedPreferences.getInstance().disableKeyCheckerForTesting();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGenerationAndRestorationOfUuid() {
        String preferenceKey = "some_preference_key";
        String expectedUniqueId = "myUuid";
        UuidBasedUniqueIdentificationGenerator generator =
                createSpiedGenerator(preferenceKey, expectedUniqueId);

        // Get a unique ID and ensure it is as expected.
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));
        verify(generator, times(1)).getUUID();

        // Asking for a unique ID again, should not try to regenerate it.
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));
        verify(generator, times(1)).getUUID();

        // After a restart, the TestGenerator should read the UUID from a preference, instead of
        // asking for it.
        generator = createSpiedGenerator(preferenceKey, null);
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));
        verify(generator, never()).getUUID();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testTwoDifferentGeneratorsShouldUseDifferentPreferences() {
        String preferenceKey1 = "some_preference_key";
        String preferenceKey2 = "some_other_preference_key";
        String expectedUniqueId1 = "myUuid";
        String expectedUniqueId2 = "myOtherUuid";
        UuidBasedUniqueIdentificationGenerator generator1 =
                createSpiedGenerator(preferenceKey1, expectedUniqueId1);
        UuidBasedUniqueIdentificationGenerator generator2 =
                createSpiedGenerator(preferenceKey2, expectedUniqueId2);

        // Get a unique ID and ensure it is as expected.
        Assert.assertEquals(expectedUniqueId1, generator1.getUniqueId(null));
        Assert.assertEquals(expectedUniqueId2, generator2.getUniqueId(null));
        verify(generator1, times(1)).getUUID();
        verify(generator2, times(1)).getUUID();

        // Asking for a unique ID again, should not try to regenerate it.
        Assert.assertEquals(expectedUniqueId1, generator1.getUniqueId(null));
        Assert.assertEquals(expectedUniqueId2, generator2.getUniqueId(null));
        verify(generator1, times(1)).getUUID();
        verify(generator2, times(1)).getUUID();
    }

    private UuidBasedUniqueIdentificationGenerator createSpiedGenerator(
            String preferenceKey, String uuidToReturn) {
        UuidBasedUniqueIdentificationGenerator spiedGenerator =
                Mockito.spy(new UuidBasedUniqueIdentificationGenerator(preferenceKey));
        doReturn(uuidToReturn).when(spiedGenerator).getUUID();
        return spiedGenerator;
    }
}
