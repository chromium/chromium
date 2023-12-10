// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.chromium.net.smoke.CronetSmokeTestRule.assertJavaEngine;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;

import java.util.List;

/**
 * Tests scenarios when the native shared library file is missing in the APK or was built for a
 * wrong architecture.
 */
@RunWith(AndroidJUnit4.class)
public class MissingNativeLibraryTest {
    @Rule public CronetSmokeTestRule mRule = new CronetPlatformSmokeTestRule();
    @Rule public ExpectedException thrown = ExpectedException.none();

    /** If the ".so" file is missing, instantiating the Cronet engine should throw an exception. */
    @Test
    @SmallTest
    public void testExceptionWhenSoFileIsAbsent() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(ApplicationProvider.getApplicationContext());
        thrown.expect(UnsatisfiedLinkError.class);
        builder.build();
    }

    /**
     * Tests the embedder ability to select Java (platform) based implementation when the native
     * library is missing or doesn't load for some reason,
     */
    @Test
    @SmallTest
    public void testForceChoiceOfJavaEngine() throws Exception {
        List<CronetProvider> availableProviders =
                CronetProvider.getAllProviders(ApplicationProvider.getApplicationContext());
        boolean foundNativeProvider = false;
        CronetProvider platformProvider = null;
        for (CronetProvider provider : availableProviders) {
            if (provider.getName().equals(CronetProvider.PROVIDER_NAME_APP_PACKAGED)) {
                assertThat(provider.isEnabled()).isTrue();
                foundNativeProvider = true;
            } else if (provider.getName().equals(CronetProvider.PROVIDER_NAME_FALLBACK)) {
                assertThat(provider.isEnabled()).isTrue();
                platformProvider = provider;
            }
        }

        assertWithMessage("Unable to find the native cronet provider")
                .that(foundNativeProvider)
                .isTrue();
        assertWithMessage("Unable to find the platform cronet provider")
                .that(platformProvider)
                .isNotNull();

        CronetEngine.Builder builder = platformProvider.createBuilder();
        CronetEngine engine = builder.build();
        assertJavaEngine(engine);

        assertWithMessage(
                        "It should be always possible to cast the created builder to"
                                + " ExperimentalCronetEngine.Builder")
                .that(builder)
                .isInstanceOf(ExperimentalCronetEngine.Builder.class);

        assertWithMessage(
                        "It should be always possible to cast the created engine to"
                                + " ExperimentalCronetEngine.Builder")
                .that(engine)
                .isInstanceOf(ExperimentalCronetEngine.class);
    }
}
