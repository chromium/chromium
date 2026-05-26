// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.test.Type;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class HostnameTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    /**
     * Tests that Cronet accepts Internationalized Domain Names (IDNs) with non-ASCII characters,
     * and translates them to their correct ASCII hostname equivalents.
     *
     * <p>Note this is intended as an end-to-end smoke test and does not thoroughly check all
     * possible edge cases of IDN translation - these are covered by various tests in //url, which
     * are not repeated here.
     */
    @Test
    @SmallTest
    // TODO(https://crbug.com/40941277): Enable for HttpEngine once we have fake hostname support
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "Uses HostResolverRules, which neither HttpEngine nor JavaCronetEngine"
                            + " support.")
    public void testIDNMapping() {
        // Note the strategic use of ß as our test character, which is handled differently based on
        // which version of IDNA is used - see Unicode Technical Standard #46.
        final var IDN_UNICODE = "example-idn-begin-ß-end";
        // On Android API <24 Cronet uses IDNA2003, under which "ß" is mapped to "ss".
        // On Android API 24+ Cronet uses IDNA2008, under which "ß" is preserved and triggers
        // punycode conversion.
        // See also https://crbug.com/513446116.
        final var EXPECTED_IDN_ASCII =
                (Build.VERSION.SDK_INT < Build.VERSION_CODES.N)
                        ? "example-idn-begin-ss-end"
                        : "xn--example-idn-begin--end-71b";

        var testFramework = mTestRule.getTestFramework();
        testFramework.applyEngineBuilderPatch(
                (builder) ->
                        builder.setExperimentalOptions(
                                new JSONObject()
                                        .put(
                                                "HostResolverRules",
                                                new JSONObject()
                                                        .put(
                                                                "host_resolver_rules",
                                                                "MAP "
                                                                        + EXPECTED_IDN_ASCII
                                                                        + " 127.0.0.1"))
                                        .toString()));
        testFramework.startEngine();

        try (var nativeTestServer = new NativeTestServer(testFramework.getContext(), Type.HTTP)) {
            nativeTestServer.start();
            var callback = new TestUrlRequestCallback();
            testFramework
                    .getEngine()
                    .newUrlRequestBuilder(
                            "http://" + IDN_UNICODE + ":" + nativeTestServer.getPort() + "/",
                            callback,
                            callback.getExecutor())
                    .build()
                    .start();
            callback.blockForDone();
            // If we managed to reach the test server using the IDN, it means the above host
            // resolver rule matched and Cronet therefore translated the IDN to the expected ASCII
            // hostname. Otherwise, the request will fail with a hostname lookup error (or a
            // "cleartext traffic not permitted" error as the resulting hostname won't be listed in
            // the Network Security Config)
            assertThat(callback.getResponseInfo()).isNotNull();
        }
    }
}
