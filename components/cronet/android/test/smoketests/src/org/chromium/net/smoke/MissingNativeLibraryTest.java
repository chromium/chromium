// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static org.chromium.net.smoke.CronetSmokeTestRule.assertJavaEngine;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;

import java.util.List;

/**
 *  Tests scenarios when the native shared library file is missing in the APK or was built for a
 *  wrong architecture.
 */
@RunWith(AndroidJUnit4.class)
public class MissingNativeLibraryTest {
    @Rule
    public CronetSmokeTestRule mRule = new CronetSmokeTestRule();
    @Rule
    public ExpectedException thrown = ExpectedException.none();

    /**
     * If the ".so" file is missing, instantiating the Cronet engine should throw an exception.
     */
    @Test
    @SmallTest
    public void testExceptionWhenSoFileIsAbsent() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(InstrumentationRegistry.getTargetContext());
        thrown.expect(UnsatisfiedLinkError.class);
        builder.build();
    }

    /**
     * Tests the embedder ability to select Java (platform) based implementation when
     * the native library is missing or doesn't load for some reason,
     */
    @Test
    @SmallTest
    public void testForceChoiceOfJavaEngine() throws Exception {
        List<CronetProvider> availableProviders =
                CronetProvider.getAllProviders(InstrumentationRegistry.getTargetContext());
        boolean foundNativeProvider = false;
        CronetProvider platformProvider = null;
        for (CronetProvider provider : availableProviders) {
            Assert.assertTrue(provider.isEnabled());
            if (provider.getName().equals(CronetProvider.PROVIDER_NAME_APP_PACKAGED)) {
                foundNativeProvider = true;
            } else if (provider.getName().equals(CronetProvider.PROVIDER_NAME_FALLBACK)) {
                platformProvider = provider;
            }
        }

        Assert.assertTrue("Unable to find the native cronet provider", foundNativeProvider);
        Assert.assertNotNull("Unable to find the platform cronet provider", platformProvider);

        CronetEngine.Builder builder = platformProvider.createBuilder();
        CronetEngine engine = builder.build();
        assertJavaEngine(engine);

        Assert.assertTrue("It should be always possible to cast the created builder to"
                        + " ExperimentalCronetEngine.Builder",
                builder instanceof ExperimentalCronetEngine.Builder);

        Assert.assertTrue("It should be always possible to cast the created engine to"
                        + " ExperimentalCronetEngine.Builder",
                engine instanceof ExperimentalCronetEngine);
    }
}
