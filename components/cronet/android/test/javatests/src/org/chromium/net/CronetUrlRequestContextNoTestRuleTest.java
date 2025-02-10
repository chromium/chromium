// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetUrlRequestContextTest.TestBadLibraryLoader;

/* This test class is equivalent to CronetUrlRequestContext except that
it does not use CronetTestRule. */
@DoNotBatch(reason = "See the comment in testSetLibraryLoaderIsEnforcedByDefaultEmbeddedProvider")
@RunWith(AndroidJUnit4.class)
public class CronetUrlRequestContextNoTestRuleTest {
    private static final String TAG = "CronetUrlRequestContextNoTestRuleTest";

    @Test
    @SmallTest
    public void testSetLibraryLoaderIsEnforcedByDefaultEmbeddedProvider() throws Exception {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(ApplicationProvider.getApplicationContext());
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);

        assertThrows(
                "Native library should not be loaded", UnsatisfiedLinkError.class, builder::build);
        assertThat(loader.wasCalled()).isTrue();
        // The init thread is started *before* the library is loaded, so the init thread is running
        // despite the library loading failure. Init thread initialization can race against test
        // cleanup (e.g. Context access). This is why we can't batch this test-suite as we must
        // restart the APK to ensure proper clean-up.
    }
}
