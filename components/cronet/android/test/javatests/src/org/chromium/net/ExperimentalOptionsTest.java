// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.SERVER_CERT_PEM;
import static org.chromium.net.CronetTestRule.SERVER_KEY_PKCS8_PEM;
import static org.chromium.net.CronetTestRule.assertContains;
import static org.chromium.net.CronetTestRule.getContext;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.support.test.runner.AndroidJUnit4;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URL;
import java.util.concurrent.CountDownLatch;

/**
 * Tests for experimental options.
 */
@RunWith(AndroidJUnit4.class)
@JNINamespace("cronet")
public class ExperimentalOptionsTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();
    @Rule
    public ExpectedException expectedException = ExpectedException.none();

    private static final String TAG = ExperimentalOptionsTest.class.getSimpleName();
    private ExperimentalCronetEngine.Builder mBuilder;
    private CountDownLatch mHangingUrlLatch;

    @Before
    public void setUp() throws Exception {
        mBuilder = new ExperimentalCronetEngine.Builder(getContext());
        mHangingUrlLatch = new CountDownLatch(1);
        CronetTestUtil.setMockCertVerifierForTesting(
                mBuilder, QuicTestServer.createMockCertVerifier());
        assertTrue(Http2TestServer.startHttp2TestServer(
                getContext(), SERVER_CERT_PEM, SERVER_KEY_PKCS8_PEM, mHangingUrlLatch));
    }

    @After
    public void tearDown() throws Exception {
        mHangingUrlLatch.countDown();
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
    }

    @Test
    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that NetLog writes effective experimental options to NetLog.
    public void testNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File logfile = File.createTempFile("cronet", "json", directory);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());

        CronetEngine cronetEngine = mBuilder.build();
        cronetEngine.startNetLogToFile(logfile.getPath(), false);
        String url = Http2TestServer.getEchoMethodUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
        cronetEngine.stopNetLog();
        assertFileContainsString(logfile, "HostResolverRules");
        assertTrue(logfile.delete());
        assertFalse(logfile.exists());
        cronetEngine.shutdown();
    }

    @DisabledTest(message = "crbug.com/1021941")
    @Test
    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSetSSLKeyLogFile() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        File dir = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("ssl_key_log_file", "", dir);

        JSONObject experimentalOptions = new JSONObject().put("ssl_key_log_file", file.getPath());
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        CronetEngine cronetEngine = mBuilder.build();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);

        assertFileContainsString(file, "CLIENT_RANDOM");
        assertTrue(file.delete());
        assertFalse(file.exists());
        cronetEngine.shutdown();
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
        assertTrue("file content doesn't match", contains);
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
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that basic Cronet functionality works when host cache persistence is enabled, and that
    // persistence works.
    public void testHostCachePersistence() throws Exception {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(getContext());

        String realUrl = testServer.getURL("/echo?status=200");
        URL javaUrl = new URL(realUrl);
        String realHost = javaUrl.getHost();
        int realPort = javaUrl.getPort();
        String testHost = "host-cache-test-host";
        String testUrl = new URL("http", testHost, realPort, javaUrl.getPath()).toString();

        mBuilder.setStoragePath(getTestStorage(getContext()))
                .enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 0);

        // Set a short delay so the pref gets written quickly.
        JSONObject staleDns = new JSONObject()
                                      .put("enable", true)
                                      .put("delay_ms", 0)
                                      .put("allow_other_network", true)
                                      .put("persist_to_disk", true)
                                      .put("persist_delay_ms", 0);
        JSONObject experimentalOptions = new JSONObject().put("StaleDNS", staleDns);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        CronetUrlRequestContext context = (CronetUrlRequestContext) mBuilder.build();

        // Create a HostCache entry for "host-cache-test-host".
        nativeWriteToHostCache(context.getUrlRequestContextAdapter(), realHost);

        // Do a request for the test URL to make sure it's cached.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                context.newUrlRequestBuilder(testUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertNull(callback.mError);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());

        // Shut down the context, persisting contents to disk, and build a new one.
        context.shutdown();
        context = (CronetUrlRequestContext) mBuilder.build();

        // Use the test URL without creating a new cache entry first. It should use the persisted
        // one.
        callback = new TestUrlRequestCallback();
        builder = context.newUrlRequestBuilder(testUrl, callback, callback.getExecutor());
        urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertNull(callback.mError);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        context.shutdown();
    }

    @Test
    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Experimental options should be specified through a JSON compliant string. When that is not
    // the case building a Cronet engine should fail when it is allowed to do so.
    public void testWrongJsonExperimentalOptions() throws Exception {
        if (nativeExperimentalOptionsParsingIsAllowedToFail()) {
            expectedException.expect(IllegalArgumentException.class);
            expectedException.expectMessage("Experimental options parsing failed");
        }
        mBuilder.setExperimentalOptions("Not a serialized JSON object");
        CronetEngine cronetEngine = mBuilder.build();
    }

    @Test
    @MediumTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testDetectBrokenConnection() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        int heartbeatIntervalSecs = 1;
        JSONObject experimentalOptions =
                new JSONObject().put("bidi_stream_detect_broken_connection", heartbeatIntervalSecs);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        ExperimentalCronetEngine cronetEngine = (ExperimentalCronetEngine) mBuilder.build();

        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        ExperimentalBidirectionalStream.Builder builder =
                cronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET");
        BidirectionalStream stream = builder.build();
        stream.start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
        cronetEngine.shutdown();
    }

    @DisabledTest(message = "crbug.com/1320725")
    @Test
    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testDetectBrokenConnectionOnNetworkFailure() throws Exception {
        // HangingRequestUrl stops the server from replying until mHangingUrlLatch is opened,
        // simulating a network failure between client and server.
        String hangingUrl = Http2TestServer.getHangingRequestUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        int heartbeatIntervalSecs = 1;
        JSONObject experimentalOptions =
                new JSONObject().put("bidi_stream_detect_broken_connection", heartbeatIntervalSecs);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        ExperimentalCronetEngine cronetEngine = (ExperimentalCronetEngine) mBuilder.build();
        cronetEngine.addRequestFinishedListener(requestFinishedListener);
        ExperimentalBidirectionalStream.Builder builder =
                cronetEngine
                        .newBidirectionalStreamBuilder(hangingUrl, callback, callback.getExecutor())
                        .setHttpMethod("GET");
        BidirectionalStream stream = builder.build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertTrue(callback.mOnErrorCalled);
        assertContains("Exception in BidirectionalStream: net::ERR_HTTP2_PING_FAILED",
                callback.mError.getMessage());
        assertEquals(NetError.ERR_HTTP2_PING_FAILED,
                ((NetworkException) callback.mError).getCronetInternalErrorCode());
        cronetEngine.shutdown();
    }

    // Sets a host cache entry with hostname "host-cache-test-host" and an AddressList containing
    // the provided address.
    private static native void nativeWriteToHostCache(long adapter, String address);
    // Whether Cronet engine creation can fail due to failure during experimental options parsing.
    private static native boolean nativeExperimentalOptionsParsingIsAllowedToFail();
}
