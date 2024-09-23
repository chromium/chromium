// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.CronetTestRule.getTestStorage;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.annotation.OptIn;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.DisableAutomaticNetLog;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetUrlRequestContext;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URL;
import java.util.concurrent.CountDownLatch;

/** Tests for experimental options. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@JNINamespace("cronet")
@OptIn(
        markerClass = {
            ConnectionMigrationOptions.Experimental.class,
            DnsOptions.Experimental.class,
            QuicOptions.Experimental.class,
            QuicOptions.QuichePassthroughOption.class
        })
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "Fallback and AOSP implementations do not support JSON experimental options")
public class ExperimentalOptionsTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();
    @Rule public ExpectedException expectedException = ExpectedException.none();

    private static final String TAG = ExperimentalOptionsTest.class.getSimpleName();
    private CountDownLatch mHangingUrlLatch;

    @Before
    public void setUp() throws Exception {
        mHangingUrlLatch = new CountDownLatch(1);
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    CronetTestUtil.setMockCertVerifierForTesting(
                                            builder, QuicTestServer.createMockCertVerifier()));
        }
        assertThat(
                        Http2TestServer.startHttp2TestServer(
                                mTestRule.getTestFramework().getContext(), mHangingUrlLatch))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        mHangingUrlLatch.countDown();
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @MediumTest
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    // Tests that NetLog writes effective experimental options to NetLog.
    public void testNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File logfile = File.createTempFile("cronet", "json", directory);

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject().put("HostResolverRules", hostResolverParams);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        cronetEngine.startNetLogToFile(logfile.getPath(), false);
        String url = Http2TestServer.getEchoMethodUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        cronetEngine.stopNetLog();
        assertFileContainsString(logfile, "HostResolverRules");
        assertThat(logfile.delete()).isTrue();
        assertThat(logfile.exists()).isFalse();
    }

    @Test
    @MediumTest
    public void testSetSSLKeyLogFile() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        File dir = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("ssl_key_log_file", "", dir);

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            JSONObject experimentalOptions =
                                    new JSONObject().put("ssl_key_log_file", file.getPath());
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");

        assertFileContainsString(file, "CLIENT_HANDSHAKE_TRAFFIC_SECRET");
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    // Helper method to assert that file contains content. It retries 5 times
    // with a 100ms interval.
    private void assertFileContainsString(File file, String content) throws Exception {
        boolean contains = false;
        for (int i = 0; i < 5; i++) {
            contains = fileContainsString(file, content);
            if (contains) break;
            Log.i(TAG, "Retrying...");
            Thread.sleep(100);
        }
        assertWithMessage("file content doesn't match").that(contains).isTrue();
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(File file, String content) throws IOException {
        FileInputStream fileInputStream = null;
        Log.i(TAG, "looking for [%s] in %s", content, file.getName());
        try {
            fileInputStream = new FileInputStream(file);
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            String actual = new String(data, "UTF-8");
            boolean contains = actual.contains(content);
            if (!contains) {
                Log.i(TAG, "file content [%s]", actual);
            }
            return contains;
        } catch (FileNotFoundException e) {
            // Ignored this exception since the file will only be created when updates are
            // flushed to the disk.
            Log.i(TAG, "file not found");
        } finally {
            if (fileInputStream != null) {
                fileInputStream.close();
            }
        }
        return false;
    }

    @Test
    @MediumTest
    // Tests that basic Cronet functionality works when host cache persistence is enabled, and that
    // persistence works.
    public void testHostCachePersistence() throws Exception {
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());

        String realUrl = NativeTestServer.getFileURL("/echo?status=200");
        URL javaUrl = new URL(realUrl);
        String realHost = javaUrl.getHost();
        int realPort = javaUrl.getPort();
        String testHost = "host-cache-test-host";
        String testUrl = new URL("http", testHost, realPort, javaUrl.getPath()).toString();

        ExperimentalCronetEngine.Builder builder =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext());

        builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()))
                .enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 0);

        // Set a short delay so the pref gets written quickly.
        JSONObject staleDns =
                new JSONObject()
                        .put("enable", true)
                        .put("delay_ms", 0)
                        .put("allow_other_network", true)
                        .put("persist_to_disk", true)
                        .put("persist_delay_ms", 0);
        JSONObject experimentalOptions = new JSONObject().put("StaleDNS", staleDns);
        builder.setExperimentalOptions(experimentalOptions.toString());
        CronetUrlRequestContext context = (CronetUrlRequestContext) builder.build();

        // Create a HostCache entry for "host-cache-test-host".
        ExperimentalOptionsTestJni.get()
                .writeToHostCache(context.getUrlRequestContextAdapter(), realHost);

        // Do a request for the test URL to make sure it's cached.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest urlRequest =
                context.newUrlRequestBuilder(testUrl, callback, callback.getExecutor()).build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);

        // Shut down the context, persisting contents to disk, and build a new one.
        context.shutdown();
        context = (CronetUrlRequestContext) builder.build();

        // Use the test URL without creating a new cache entry first. It should use the persisted
        // one.
        callback = new TestUrlRequestCallback();
        urlRequest =
                context.newUrlRequestBuilder(testUrl, callback, callback.getExecutor()).build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        context.shutdown();
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @MediumTest
    // Experimental options should be specified through a JSON compliant string. When that is not
    // the case building a Cronet engine should fail.
    public void testWrongJsonExperimentalOptions() throws Exception {
        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                mTestRule
                                        .getTestFramework()
                                        .applyEngineBuilderPatch(
                                                (builder) ->
                                                        builder.setExperimentalOptions(
                                                                "Not a serialized JSON object")));
        // The top level exception is a side effect of using applyEngineBuilderPatch
        assertThat(e).hasCauseThat().isInstanceOf(IllegalArgumentException.class);
        assertThat(e)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Experimental options parsing failed");
    }

    @Test
    @MediumTest
    public void testDetectBrokenConnection() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            int heartbeatIntervalSecs = 1;
                            JSONObject experimentalOptions =
                                    new JSONObject()
                                            .put(
                                                    "bidi_stream_detect_broken_connection",
                                                    heartbeatIntervalSecs);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        ExperimentalBidirectionalStream.Builder builder =
                cronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET");
        BidirectionalStream stream = builder.build();
        stream.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
    }

    @DisabledTest(message = "crbug.com/1320725")
    @Test
    @LargeTest
    public void testDetectBrokenConnectionOnNetworkFailure() throws Exception {
        // HangingRequestUrl stops the server from replying until mHangingUrlLatch is opened,
        // simulating a network failure between client and server.
        String hangingUrl = Http2TestServer.getHangingRequestUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            int heartbeatIntervalSecs = 1;
                            JSONObject experimentalOptions =
                                    new JSONObject()
                                            .put(
                                                    "bidi_stream_detect_broken_connection",
                                                    heartbeatIntervalSecs);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.addRequestFinishedListener(requestFinishedListener);
        ExperimentalBidirectionalStream.Builder builder =
                cronetEngine
                        .newBidirectionalStreamBuilder(hangingUrl, callback, callback.getExecutor())
                        .setHttpMethod("GET");
        BidirectionalStream stream = builder.build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in BidirectionalStream: net::ERR_HTTP2_PING_FAILED");
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(NetError.ERR_HTTP2_PING_FAILED);
        cronetEngine.shutdown();
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        // Sets a host cache entry with hostname "host-cache-test-host" and an AddressList
        // containing the provided address.
        void writeToHostCache(long adapter, String address);

        // Whether Cronet engine creation can fail due to failure during experimental options
        // parsing.
        boolean experimentalOptionsParsingIsAllowedToFail();
    }
}
