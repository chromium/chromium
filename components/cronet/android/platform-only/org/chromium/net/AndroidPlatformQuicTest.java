// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.Manifest.permission.NETWORK_SETTINGS;
import static android.net.ConnectivityManager.FIREWALL_CHAIN_BACKGROUND;
import static android.net.ConnectivityManager.FIREWALL_RULE_DENY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestUtil.assertIsQuic;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.app.UiAutomation;
import android.content.Context;
import android.net.ConnectivityManager;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.BoolFlag;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.impl.CronetLibraryLoader;

import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;

/**
 * Tests QUIC request when Cronet is build in the Android platform (HttpEngine)
 *
 * <p>Note that these tests assume Cronet and tests can access Connectivity module hidden APIs. So,
 * this code is only built when Cronet tests are built for the Android platform.
 */
@DoNotBatch(reason = "crbug.com/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason =
                "The fallback implementation doesn't support QUIC. "
                        + "crbug.com/1494870: Enable for AOSP_PLATFORM once fixed")
public class AndroidPlatformQuicTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private static final int CONNECTION_CLOSE_TIMEOUT_MS = 1_000;

    @Before
    public void setUp() throws Exception {
        // Load library first, since we need the Quic test server's URL.
        CronetLibraryLoader.switchToTestLibrary();
        CronetLibraryLoader.loadLibrary();
        QuicTestServer.startQuicTestServer(mTestRule.getTestFramework().getContext());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.addQuicHint(
                                    QuicTestServer.getServerHost(),
                                    QuicTestServer.getServerPort(),
                                    QuicTestServer.getServerPort());

                            JSONObject quicParams =
                                    new JSONObject()
                                            // Set long timeout to prevent connection closures due
                                            // to timeout
                                            .put("idle_connection_timeout_seconds", 3600);
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject()
                                            .put("QUIC", quicParams)
                                            .put("HostResolverRules", hostResolverParams);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                            CronetTestUtil.setMockCertVerifierForTesting(
                                    builder, QuicTestServer.createMockCertVerifier());
                        });
        mTestRule.getTestFramework().startEngine();
    }

    @After
    public void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = "ChromiumBaseFeature_kQuicRegisterConnectionClosePayload",
                        value = true)
            })
    // FIREWALL_CHAIN_BACKGROUND is supported on V+
    @RequiresMinAndroidApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testQuicRegisterConnectionClose() throws Exception {
        final ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().getEngine();
        final String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        final UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        final String expectedContent = "This is a simple text file served by QUIC.\n";
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(expectedContent);
        assertIsQuic(callback.getResponseInfoWithChecks());

        // Ensure the test server has established one QUIC connection.
        assertEquals(QuicTestServer.numSessions(), 1);

        final ConnectivityManager cm =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);

        final int myUid = Process.myUid();
        final int chain = FIREWALL_CHAIN_BACKGROUND;
        final boolean wasChainEnabled =
                runWithShellPermissionIdentity(
                        () -> cm.getFirewallChainEnabled(chain), NETWORK_SETTINGS);
        final int previousUidFirewallRule =
                runWithShellPermissionIdentity(
                        () -> cm.getUidFirewallRule(chain, myUid), NETWORK_SETTINGS);

        try {
            // Block network access and trigger the closing of the QUIC connection.
            runWithShellPermissionIdentity(
                    () -> {
                        cm.setFirewallChainEnabled(chain, true);
                        cm.setUidFirewallRule(chain, myUid, FIREWALL_RULE_DENY);
                    },
                    NETWORK_SETTINGS);

            // Ensure the test server closes the QUIC connection
            waitForConnectionClose();
        } finally {
            runWithShellPermissionIdentity(
                    () -> {
                        // Restore the previous firewall status
                        cm.setFirewallChainEnabled(chain, wasChainEnabled);
                        try {
                            cm.setUidFirewallRule(chain, myUid, previousUidFirewallRule);
                        } catch (IllegalStateException ignored) {
                            // Removing match causes an exception when the rule entry for the uid
                            // does not exist. But this is fine and can be ignored.
                        }
                    },
                    NETWORK_SETTINGS);
        }

        // Verify that a new request after the connection closure succeeds.
        final TestUrlRequestCallback callback2 = new TestUrlRequestCallback();

        final UrlRequest.Builder requestBuilder2 =
                cronetEngine.newUrlRequestBuilder(quicURL, callback2, callback2.getExecutor());
        requestBuilder2.build().start();
        callback2.blockForDone();

        assertThat(callback2.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback2.mResponseAsString).isEqualTo(expectedContent);
        assertIsQuic(callback2.getResponseInfoWithChecks());

        cronetEngine.shutdown();
    }

    private void waitForConnectionClose() throws InterruptedException {
        final long timeout = SystemClock.elapsedRealtime() + CONNECTION_CLOSE_TIMEOUT_MS;
        while (timeout > SystemClock.elapsedRealtime()) {
            if (QuicTestServer.numSessions() == 0) {
                return;
            }
            Thread.sleep(100);
        }
        fail("QUIC connection did not close in " + CONNECTION_CLOSE_TIMEOUT_MS + "ms");
    }

    // Calls a supplier adopting Shell's permissions, and returning the result.
    public static <T> T runWithShellPermissionIdentity(
            Supplier<T> runnable, String... permissions) {
        AtomicReference<T> result = new AtomicReference<>();
        runWithShellPermissionIdentity(() -> result.set(runnable.get()), permissions);
        return result.get();
    }

    // Runs a runnable adopting Shell's permissions.
    public static void runWithShellPermissionIdentity(Runnable runnable, String... permissions) {
        final UiAutomation automan = getInstrumentation().getUiAutomation();
        automan.adoptShellPermissionIdentity(permissions);
        try {
            runnable.run();
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }
}
