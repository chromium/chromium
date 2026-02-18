// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeFalse;

import static org.chromium.net.smoke.CronetSmokeTestRule.assertJavaEngine;
import static org.chromium.net.smoke.CronetSmokeTestRule.assertSuccessfulNonEmptyResponse;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.UrlRequest;
import org.chromium.net.impl.HttpEngineNativeProvider;

/** Tests scenario when an app doesn't contain the native Cronet implementation. */
@RunWith(AndroidJUnit4.class)
public class PlatformOnlyEngineTest {
    private String mURL;
    private TestSupport.TestServer mServer;

    @Rule public CronetSmokeTestRule mRule = new CronetPlatformSmokeTestRule();

    @Before
    public void setUp() throws Exception {
        // Java-only implementation of the Cronet engine only supports Http/1 protocol.
        mServer =
                mRule.getTestSupport()
                        .createTestServer(
                                ApplicationProvider.getApplicationContext(),
                                TestSupport.Protocol.HTTP1);
        assertThat(mServer.start()).isTrue();
        mURL = mServer.getSuccessURL();
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    /** Test a successful response when a request is sent by the Java Cronet Engine. */
    @Test
    @SmallTest
    public void testSuccessfulResponse() {
        // On Android 14 (U) and above, HttpEngine is available. This makes this test—and
        // its associated APK—obsolete, as it is designed to simulate a "Fallback-only" environment
        // that no longer exists on these versions.
        assumeFalse(
                "This test runs only on Android devices that do not have HttpEngine available.",
                HttpEngineNativeProvider.isHttpEngineAvailable());
        mRule.initCronetEngine();
        assertJavaEngine(mRule.getCronetEngine());
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder =
                mRule.getCronetEngine()
                        .newUrlRequestBuilder(mURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertSuccessfulNonEmptyResponse(callback, mURL);
    }
}
