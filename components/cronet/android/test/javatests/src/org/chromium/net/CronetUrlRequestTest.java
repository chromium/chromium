// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.assertContains;
import static org.chromium.net.CronetTestRule.getContext;

import android.os.Build;
import android.os.ConditionVariable;
import android.os.Process;
import android.os.StrictMode;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.TestUrlRequestCallback.FailureType;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.impl.CronetUrlRequest;
import org.chromium.net.impl.UrlResponseInfoImpl;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.FailurePhase;
import org.chromium.net.test.ServerCertificate;

import java.io.IOException;
import java.net.ConnectException;
import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test functionality of CronetUrlRequest.
 */
@RunWith(AndroidJUnit4.class)
public class CronetUrlRequestTest {
    private static final String TAG = CronetUrlRequestTest.class.getSimpleName();

    // URL used for base tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";

    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetTestFramework mTestFramework;
    private MockUrlRequestJobFactory mMockUrlRequestJobFactory;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        // Add url interceptors after native application context is initialized.
        if (!mTestRule.testingJavaImpl()) {
            mMockUrlRequestJobFactory = new MockUrlRequestJobFactory(mTestFramework.mCronetEngine);
        }
    }

    @After
    public void tearDown() throws Exception {
        if (!mTestRule.testingJavaImpl()) {
            mMockUrlRequestJobFactory.shutdown();
        }
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.mCronetEngine.shutdown();
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Create request.
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        // Wait for all posted tasks to be executed to ensure there is no unhandled exception.
        callback.shutdownExecutorAndWait();
        assertTrue(urlRequest.isDone());
        return callback;
    }

    private void checkResponseInfo(UrlResponseInfo responseInfo, String expectedUrl,
            int expectedHttpStatusCode, String expectedHttpStatusText) {
        assertThat(responseInfo.getUrl()).isEqualTo(expectedUrl);
        assertThat(responseInfo.getUrlChain().get(responseInfo.getUrlChain().size() - 1))
                .isEqualTo(expectedUrl);
        assertThat(responseInfo.getHttpStatusCode()).isEqualTo(expectedHttpStatusCode);
        assertThat(responseInfo.getHttpStatusText()).isEqualTo(expectedHttpStatusText);
        assertFalse(responseInfo.wasCached());
        assertThat(responseInfo.toString()).isNotEmpty();
    }

    private void checkResponseInfoHeader(
            UrlResponseInfo responseInfo, String headerName, String headerValue) {
        Map<String, List<String>> responseHeaders =
                responseInfo.getAllHeaders();
        List<String> header = responseHeaders.get(headerName);
        assertNotNull(header);
        assertTrue(header.contains(headerValue));
    }

    @Test
    @SmallTest
    public void testBuilderChecks() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        try {
            mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    null, callback, callback.getExecutor());
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("URL is required.");
        }
        try {
            mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    NativeTestServer.getRedirectURL(), null, callback.getExecutor());
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Callback is required.");
        }
        try {
            mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    NativeTestServer.getRedirectURL(), callback, null);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Executor is required.");
        }
        // Verify successful creation doesn't throw.
        mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectURL(), callback, callback.getExecutor());
    }

    @Test
    @SmallTest
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        UrlResponseInfo urlResponseInfo = createUrlResponseInfo(new String[] {url}, "OK", 200, 86,
                "Connection", "close", "Content-Length", "3", "Content-Type", "text/plain");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.mResponseInfo);
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");
    }

    UrlResponseInfo createUrlResponseInfo(
            String[] urls, String message, int statusCode, int receivedBytes, String... headers) {
        ArrayList<Map.Entry<String, String>> headersList = new ArrayList<>();
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                    headers[i], headers[i + 1]));
        }
        UrlResponseInfoImpl unknown = new UrlResponseInfoImpl(Arrays.asList(urls), statusCode,
                message, headersList, false, "unknown", ":0", receivedBytes);
        return unknown;
    }

    void runConnectionMigrationTest(boolean disableConnectionMigration) {
        // URLRequest load flags at net/base/load_flags_list.h.
        int connectionMigrationLoadFlag =
                CronetUrlRequestTestJni.get().getConnectionMigrationDisableLoadFlag();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        // Create builder, start a request, and check if default load_flags are set correctly.
        ExperimentalUrlRequest.Builder builder =
                (ExperimentalUrlRequest.Builder) mTestFramework.mCronetEngine.newUrlRequestBuilder(
                        NativeTestServer.getFileURL("/success.txt"), callback,
                        callback.getExecutor());
        // Disable connection migration.
        if (disableConnectionMigration) builder.disableConnectionMigration();
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();
        int loadFlags = CronetTestUtil.getLoadFlags(urlRequest);
        if (disableConnectionMigration) {
            assertThat(loadFlags & connectionMigrationLoadFlag)
                    .isEqualTo(connectionMigrationLoadFlag);
        } else {
            assertThat(loadFlags & connectionMigrationLoadFlag).isEqualTo(0);
        }
        callback.setAutoAdvance(true);
        callback.startNextRead(urlRequest);
        callback.blockForDone();
    }

    /**
     * Tests that disabling connection migration sets the URLRequest load flag correctly.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLoadFlagsWithConnectionMigration() throws Exception {
        runConnectionMigrationTest(/*disableConnectionMigration=*/false);
        runConnectionMigrationTest(/*disableConnectionMigration=*/true);
    }

    /**
     * Tests a redirect by running it step-by-step. Also tests that delaying a
     * request works as expected. To make sure there are no unexpected pending
     * messages, does a GET between UrlRequest.Callback callbacks.
     */
    @Test
    @SmallTest
    public void testRedirectAsync() throws Exception {
        // Start the request and wait to see the redirect.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectURL(), callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        // Check the redirect.
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RECEIVED_REDIRECT);
        assertThat(callback.mRedirectResponseInfoList).hasSize(1);
        checkResponseInfo(callback.mRedirectResponseInfoList.get(0),
                NativeTestServer.getRedirectURL(), 302, "Found");
        assertThat(callback.mRedirectResponseInfoList.get(0).getUrlChain()).hasSize(1);
        assertThat(callback.mRedirectUrlList.get(0)).isEqualTo(NativeTestServer.getSuccessURL());
        checkResponseInfoHeader(
                callback.mRedirectResponseInfoList.get(0), "redirect-header", "header-value");

        UrlResponseInfo expected =
                createUrlResponseInfo(new String[] {NativeTestServer.getRedirectURL()}, "Found",
                        302, 73, "Location", "/success.txt", "redirect-header", "header-value");
        mTestRule.assertResponseEquals(expected, callback.mRedirectResponseInfoList.get(0));

        // Wait for an unrelated request to finish. The request should not
        // advance until followRedirect is invoked.
        testSimpleGet();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RECEIVED_REDIRECT);
        assertThat(callback.mRedirectResponseInfoList).hasSize(1);

        // Follow the redirect and wait for the next set of headers.
        urlRequest.followRedirect();
        callback.waitForNextStep();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        assertThat(callback.mRedirectResponseInfoList).hasSize(1);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getSuccessURL(), 200, "OK");
        assertThat(callback.mResponseInfo.getUrlChain())
                .containsExactly(
                        NativeTestServer.getRedirectURL(), NativeTestServer.getSuccessURL())
                .inOrder();

        // Wait for an unrelated request to finish. The request should not
        // advance until read is invoked.
        testSimpleGet();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        // One read should get all the characters, but best not to depend on
        // how much is actually read from the socket at once.
        while (!callback.isDone()) {
            callback.startNextRead(urlRequest);
            callback.waitForNextStep();
            String response = callback.mResponseAsString;
            ResponseStep step = callback.mResponseStep;
            if (!callback.isDone()) {
                assertThat(step).isEqualTo(ResponseStep.ON_READ_COMPLETED);
            }
            // Should not receive any messages while waiting for another get,
            // as the next read has not been started.
            testSimpleGet();
            assertThat(callback.mResponseAsString).isEqualTo(response);
            assertThat(callback.mResponseStep).isEqualTo(step);
        }
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(callback.mResponseAsString).isEqualTo(NativeTestServer.SUCCESS_BODY);

        UrlResponseInfo urlResponseInfo = createUrlResponseInfo(
                new String[] {NativeTestServer.getRedirectURL(), NativeTestServer.getSuccessURL()},
                "OK", 200, 258, "Content-Type", "text/plain", "Access-Control-Allow-Origin", "*",
                "header-name", "header-value", "multi-header-name", "header-value1",
                "multi-header-name", "header-value2");

        mTestRule.assertResponseEquals(urlResponseInfo, callback.mResponseInfo);
        // Make sure there are no other pending messages, which would trigger
        // asserts in TestUrlRequestCallback.
        testSimpleGet();
    }

    /**
     * Tests redirect without location header doesn't cause a crash.
     */
    @Test
    @SmallTest
    public void testRedirectWithNullLocationHeader() throws Exception {
        String url = NativeTestServer.getFileURL("/redirect_broken_header.html");
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        final UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertThat(callback.mResponseAsString)
                .isEqualTo("<!DOCTYPE html>\n<html>\n<head>\n<title>Redirect</title>\n"
                        + "<p>Redirecting...</p>\n</head>\n</html>\n");
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(302);
        assertNull(callback.mError);
        assertFalse(callback.mOnErrorCalled);
    }

    /**
     * Tests onRedirectReceived after cancel doesn't cause a crash.
     */
    @Test
    @SmallTest
    public void testOnRedirectReceivedAfterCancel() throws Exception {
        final AtomicBoolean failedExpectation = new AtomicBoolean();
        TestUrlRequestCallback callback = new TestUrlRequestCallback() {
            @Override
            public void onRedirectReceived(
                    UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
                assertThat(mRedirectCount).isEqualTo(0);
                failedExpectation.compareAndSet(false, 0 != mRedirectCount);
                super.onRedirectReceived(request, info, newLocationUrl);
                // Cancel the request, so the second redirect will not be received.
                request.cancel();
            }

            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onReadCompleted(
                    UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onCanceled(UrlRequest request, UrlResponseInfo info) {
                assertThat(mRedirectCount).isEqualTo(1);
                failedExpectation.compareAndSet(false, 1 != mRedirectCount);
                super.onCanceled(request, info);
            }
        };

        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getMultiRedirectURL(), callback, callback.getExecutor());

        final UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertFalse(failedExpectation.get());
        // Check that only one redirect is received.
        assertThat(callback.mRedirectCount).isEqualTo(1);
        // Check that onCanceled is called.
        assertTrue(callback.mOnCanceledCalled);
    }

    @Test
    @SmallTest
    public void testNotFound() throws Exception {
        String url = NativeTestServer.getFileURL("/notfound.html");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        checkResponseInfo(callback.mResponseInfo, url, 404, "Not Found");
        assertThat(callback.mResponseAsString)
                .isEqualTo("<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n"
                        + "<p>Test page loaded.</p>\n</head>\n</html>\n");
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
    }

    // Checks that UrlRequest.Callback.onFailed is only called once in the case
    // of ERR_CONTENT_LENGTH_MISMATCH, which has an unusual failure path.
    // See http://crbug.com/468803.
    @Test
    @SmallTest
    @OnlyRunNativeCronet // No canonical exception to assert on
    public void testContentLengthMismatchFailsOnce() throws Exception {
        String url = NativeTestServer.getFileURL("/content_length_mismatch.html");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        // The entire response body will be read before the error is returned.
        // This is because the network stack returns data as it's read from the
        // socket, and the socket close message which triggers the error will
        // only be passed along after all data has been read.
        assertThat(callback.mResponseAsString)
                .isEqualTo("Response that lies about content length.");
        assertNotNull(callback.mError);
        assertContains("Exception in CronetUrlRequest: net::ERR_CONTENT_LENGTH_MISMATCH",
                callback.mError.getMessage());
        // Wait for a couple round trips to make sure there are no pending
        // onFailed messages. This test relies on checks in
        // TestUrlRequestCallback catching a second onFailed call.
        testSimpleGet();
    }

    @Test
    @SmallTest
    public void testSetHttpMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String methodName = "HEAD";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());
        // Try to set 'null' method.
        try {
            builder.setHttpMethod(null);
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Method is required.");
        }

        builder.setHttpMethod(methodName);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mHttpResponseDataLength).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testBadMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                TEST_URL, callback, callback.getExecutor());
        try {
            builder.setHttpMethod("bad:method!");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid http method bad:method!");
        }
    }

    @Test
    @SmallTest
    public void testBadHeaderName() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                TEST_URL, callback, callback.getExecutor());
        try {
            builder.addHeader("header:name", "headervalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header header:name=headervalue");
        }
    }

    @Test
    @SmallTest
    public void testAcceptEncodingIgnored() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoAllHeadersURL(), callback, callback.getExecutor());
        // This line should eventually throw an exception, once callers have migrated
        builder.addHeader("accept-encoding", "foozip");
        builder.build().start();
        callback.blockForDone();
        assertFalse(callback.mResponseAsString.contains("foozip"));
    }

    @Test
    @SmallTest
    public void testBadHeaderValue() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                TEST_URL, callback, callback.getExecutor());
        try {
            builder.addHeader("headername", "bad header\r\nvalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo(
                    "Invalid header headername=bad header\r\nvalue");
        }
    }

    @Test
    @SmallTest
    public void testAddHeader() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(headerName), callback, callback.getExecutor());

        builder.addHeader(headerName, headerValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(headerValue);
    }

    @Test
    @SmallTest
    public void testMultiRequestHeaders() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "header-name";
        String headerValue1 = "header-value1";
        String headerValue2 = "header-value2";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoAllHeadersURL(), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue1);
        builder.addHeader(headerName, headerValue2);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        String headers = callback.mResponseAsString;
        Pattern pattern = Pattern.compile(headerName + ":\\s(.*)\\r\\n");
        Matcher matcher = pattern.matcher(headers);
        List<String> actualValues = new ArrayList<String>();
        while (matcher.find()) {
            actualValues.add(matcher.group(1));
        }
        assertThat(actualValues).containsExactly("header-value2");
    }

    @Test
    @SmallTest
    public void testCustomReferer_verbatim() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String refererName = "Referer";
        String refererValue = "http://example.com/";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(refererName), callback, callback.getExecutor());
        builder.addHeader(refererName, refererValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(refererValue);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCustomReferer_changeToCanonical() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String refererName = "Referer";
        String refererValueNoTrailingSlash = "http://example.com";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(refererName), callback, callback.getExecutor());
        builder.addHeader(refererName, refererValueNoTrailingSlash);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(refererValueNoTrailingSlash + "/");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCustomReferer_discardInvalid() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String refererName = "Referer";
        String invalidRefererValue = "foobar";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(refererName), callback, callback.getExecutor());
        builder.addHeader(refererName, invalidRefererValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Header not found. :(");
    }

    @Test
    @SmallTest
    public void testCustomUserAgent() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(userAgentName), callback, callback.getExecutor());
        builder.addHeader(userAgentName, userAgentValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    public void testDefaultUserAgent() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "User-Agent";
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(headerName), callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertTrue("Default User-Agent should contain Cronet/n.n.n.n but is "
                        + callback.mResponseAsString,
                Pattern.matches(
                        ".+Cronet/\\d+\\.\\d+\\.\\d+\\.\\d+.+", callback.mResponseAsString));
    }

    @Test
    @SmallTest
    public void testMockSuccess() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mRedirectResponseInfoList).isEmpty();
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        Map<String, List<String>> responseHeaders = callback.mResponseInfo.getAllHeaders();
        assertThat(responseHeaders).containsEntry("header-name", Arrays.asList("header-value"));
        assertThat(responseHeaders)
                .containsEntry(
                        "multi-header-name", Arrays.asList("header-value1", "header-value2"));
    }

    @Test
    @SmallTest
    public void testResponseHeadersList() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        List<Map.Entry<String, String>> responseHeaders =
                callback.mResponseInfo.getAllHeadersAsList();

        assertThat(new AbstractMap.SimpleEntry<>("Content-Type", "text/plain"))
                .isEqualTo(responseHeaders.get(0));
        assertThat(new AbstractMap.SimpleEntry<>("Access-Control-Allow-Origin", "*"))
                .isEqualTo(responseHeaders.get(1));
        assertThat(new AbstractMap.SimpleEntry<>("header-name", "header-value"))
                .isEqualTo(responseHeaders.get(2));
        assertThat(new AbstractMap.SimpleEntry<>("multi-header-name", "header-value1"))
                .isEqualTo(responseHeaders.get(3));
        assertThat(new AbstractMap.SimpleEntry<>("multi-header-name", "header-value2"))
                .isEqualTo(responseHeaders.get(4));
    }

    @Test
    @SmallTest
    public void testMockMultiRedirect() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(NativeTestServer.getMultiRedirectURL());
        UrlResponseInfo mResponseInfo = callback.mResponseInfo;
        assertThat(callback.mRedirectCount).isEqualTo(2);
        assertThat(mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mRedirectResponseInfoList).hasSize(2);

        // Check first redirect (multiredirect.html -> redirect.html)
        UrlResponseInfo firstExpectedResponseInfo = createUrlResponseInfo(
                new String[] {NativeTestServer.getMultiRedirectURL()}, "Found", 302, 76, "Location",
                "/redirect.html", "redirect-header0", "header-value");
        UrlResponseInfo firstRedirectResponseInfo = callback.mRedirectResponseInfoList.get(0);
        mTestRule.assertResponseEquals(firstExpectedResponseInfo, firstRedirectResponseInfo);

        // Check second redirect (redirect.html -> success.txt)
        UrlResponseInfo secondExpectedResponseInfo = createUrlResponseInfo(
                new String[] {NativeTestServer.getMultiRedirectURL(),
                        NativeTestServer.getRedirectURL(), NativeTestServer.getSuccessURL()},
                "OK", 200, 334, "Content-Type", "text/plain", "Access-Control-Allow-Origin", "*",
                "header-name", "header-value", "multi-header-name", "header-value1",
                "multi-header-name", "header-value2");

        mTestRule.assertResponseEquals(secondExpectedResponseInfo, mResponseInfo);
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertThat(callback.mRedirectCount).isEqualTo(2);
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
    }

    @Test
    @SmallTest
    public void testMockNotFound() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(NativeTestServer.getNotFoundURL());
        UrlResponseInfo expected = createUrlResponseInfo(
                new String[] {NativeTestServer.getNotFoundURL()}, "Not Found", 404, 120);
        mTestRule.assertResponseEquals(expected, callback.mResponseInfo);
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertFalse(callback.mOnErrorCalled);
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testMockStartAsyncError() throws Exception {
        final int arbitraryNetError = -3;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.START, arbitraryNetError));
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(arbitraryNetError);
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertTrue(callback.mOnErrorCalled);
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testMockReadDataSyncError() throws Exception {
        final int arbitraryNetError = -4;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_SYNC, arbitraryNetError));
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseInfo.getReceivedByteCount()).isEqualTo(15);
        assertNotNull(callback.mError);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(arbitraryNetError);
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertTrue(callback.mOnErrorCalled);
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testMockReadDataAsyncError() throws Exception {
        final int arbitraryNetError = -5;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_ASYNC, arbitraryNetError));
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseInfo.getReceivedByteCount()).isEqualTo(15);
        assertNotNull(callback.mError);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(arbitraryNetError);
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertTrue(callback.mOnErrorCalled);
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }

    /**
     * Tests that request continues when client certificate is requested.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testMockClientCertificateRequested() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlForClientCertificateRequest());
        assertNotNull(callback.mResponseInfo);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("data");
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertNull(callback.mError);
        assertFalse(callback.mOnErrorCalled);
    }

    /**
     * Tests that an SSL cert error will be reported via {@link UrlRequest.Callback#onFailed}.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testMockSSLCertificateError() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlForSSLCertificateError());
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertTrue(callback.mOnErrorCalled);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(-201);
        assertContains("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID",
                callback.mError.getMessage());
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }

    /**
     * Tests that an SSL cert error with upload will be reported via {@link
     * UrlRequest.Callback#onFailed}.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testSSLCertificateError() throws Exception {
        EmbeddedTestServer sslServer = EmbeddedTestServer.createAndStartHTTPSServer(
                getContext(), ServerCertificate.CERT_EXPIRED);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                sslServer.getURL("/"), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertTrue(callback.mOnErrorCalled);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(-201);
        assertContains("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID",
                callback.mError.getMessage());
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);

        sslServer.stopAndDestroyServer();
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset.
     */
    @Test
    @SmallTest
    public void testSimpleGetBufferUpdates() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        // Since the default method is "GET", the expected response body is also
        // "GET".
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertThat(readBuffer.position()).isEqualTo(3);

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (callback.mResponseAsString.length() < 2) {
            assertFalse(callback.isDone());
            callback.startNextRead(urlRequest, readBuffer);
            callback.waitForNextStep();
        }

        // Make sure the two characters were read.
        assertThat(callback.mResponseAsString).isEqualTo("GE");

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORGE");
        // The limit and position should be 5.
        assertThat(readBuffer.limit()).isEqualTo(5);
        assertThat(readBuffer.position()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 4 and a limit() of 5.
        readBuffer.position(3);
        callback.startNextRead(urlRequest, readBuffer);
        callback.waitForNextStep();

        // Make sure all three characters of the response have now been read.
        assertThat(callback.mResponseAsString).isEqualTo("GET");

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORTE");

        // Make sure position and limit were updated correctly.
        assertThat(readBuffer.position()).isEqualTo(4);
        assertThat(readBuffer.limit()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        callback.startNextRead(urlRequest, readBuffer);
        callback.waitForNextStep();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");

        // Check that buffer contents were not modified.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORTE");

        // Position should not have been modified, since nothing was read.
        assertThat(readBuffer.position()).isEqualTo(1);
        // Limit should be unchanged as always.
        assertThat(readBuffer.limit()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestUrlRequestCallback.
        testSimpleGet();
    }

    @Test
    @SmallTest
    public void testBadBuffers() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        // Try to read using a full buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocateDirect(4);
            readBuffer.put("full".getBytes());
            urlRequest.read(readBuffer);
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("ByteBuffer is already full.");
        }

        // Try to read using a non-direct buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocate(5);
            urlRequest.read(readBuffer);
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("byteBuffer must be a direct ByteBuffer.");
        }

        // Finish the request with a direct ByteBuffer.
        callback.setAutoAdvance(true);
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.read(readBuffer);
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
    }

    @Test
    @SmallTest
    public void testNoIoInCancel() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        final UrlRequest urlRequest =
                mTestFramework.mCronetEngine
                        .newUrlRequestBuilder(NativeTestServer.getEchoHeaderURL("blah-header"),
                                callback, callback.getExecutor())
                        .addHeader("blah-header", "blahblahblah")
                        .build();
        urlRequest.start();
        callback.waitForNextStep();
        callback.startNextRead(urlRequest, ByteBuffer.allocateDirect(4));
        callback.waitForNextStep();
        StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                           .detectAll()
                                           .penaltyDeath()
                                           .penaltyLog()
                                           .build());
        try {
            urlRequest.cancel();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        callback.blockForDone();
        assertThat(callback.mOnCanceledCalled).isEqualTo(true);
    }

    @Test
    @SmallTest
    public void testUnexpectedReads() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        final UrlRequest urlRequest =
                mTestFramework.mCronetEngine
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectURL(), callback, callback.getExecutor())
                        .build();

        // Try to read before starting request.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
        }

        // Verify reading right after start throws an assertion. Both must be
        // invoked on the Executor thread, to prevent receiving data until after
        // startNextRead has been invoked.
        Runnable startAndRead = new Runnable() {
            @Override
            public void run() {
                urlRequest.start();
                try {
                    callback.startNextRead(urlRequest);
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                }
            }
        };
        callback.getExecutor().submit(startAndRead).get();
        callback.waitForNextStep();

        assertThat(ResponseStep.ON_RECEIVED_REDIRECT).isEqualTo(callback.mResponseStep);
        // Try to read after the redirect.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
        }
        urlRequest.followRedirect();
        callback.waitForNextStep();

        assertThat(ResponseStep.ON_RESPONSE_STARTED).isEqualTo(callback.mResponseStep);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);

        while (!callback.isDone()) {
            Runnable readTwice = new Runnable() {
                @Override
                public void run() {
                    callback.startNextRead(urlRequest);
                    // Try to read again before the last read completes.
                    try {
                        callback.startNextRead(urlRequest);
                        fail("Exception not thrown");
                    } catch (IllegalStateException e) {
                    }
                }
            };
            callback.getExecutor().submit(readTwice).get();
            callback.waitForNextStep();
        }

        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        assertThat(callback.mResponseAsString).isEqualTo(NativeTestServer.SUCCESS_BODY);

        // Try to read after request is complete.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
        }
    }

    @Test
    @SmallTest
    public void testUnexpectedFollowRedirects() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        final UrlRequest urlRequest =
                mTestFramework.mCronetEngine
                        .newUrlRequestBuilder(
                                NativeTestServer.getRedirectURL(), callback, callback.getExecutor())
                        .build();

        // Try to follow a redirect before starting the request.
        try {
            urlRequest.followRedirect();
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
        }

        // Try to follow a redirect just after starting the request. Has to be
        // done on the executor thread to avoid a race.
        Runnable startAndRead = new Runnable() {
            @Override
            public void run() {
                urlRequest.start();
                try {
                    urlRequest.followRedirect();
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                }
            }
        };
        callback.getExecutor().execute(startAndRead);
        callback.waitForNextStep();

        assertThat(ResponseStep.ON_RECEIVED_REDIRECT).isEqualTo(callback.mResponseStep);
        // Try to follow the redirect twice. Second attempt should fail.
        Runnable followRedirectTwice = new Runnable() {
            @Override
            public void run() {
                urlRequest.followRedirect();
                try {
                    urlRequest.followRedirect();
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                }
            }
        };
        callback.getExecutor().execute(followRedirectTwice);
        callback.waitForNextStep();

        assertThat(ResponseStep.ON_RESPONSE_STARTED).isEqualTo(callback.mResponseStep);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);

        while (!callback.isDone()) {
            try {
                urlRequest.followRedirect();
                fail("Exception not thrown");
            } catch (IllegalStateException e) {
            }
            callback.startNextRead(urlRequest);
            callback.waitForNextStep();
        }

        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        assertThat(callback.mResponseAsString).isEqualTo(NativeTestServer.SUCCESS_BODY);

        // Try to follow redirect after request is complete.
        try {
            urlRequest.followRedirect();
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
        }
    }

    @Test
    @SmallTest
    public void testUploadSetDataProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        try {
            builder.setUploadDataProvider(null, callback.getExecutor());
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid UploadDataProvider.");
        }

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        try {
            builder.build().start();
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
        }
    }

    @Test
    @SmallTest
    public void testUploadEmptyBodySync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(0);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(0);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEmpty();
        dataProvider.assertClosed();
    }

    @Test
    @SmallTest
    public void testUploadSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(4);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(16);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(4);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Yet another test");
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(16);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(4);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Yet another test");
    }

    @Test
    @SmallTest
    public void testUploadChangesDefaultMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("POST");
    }

    @Test
    @SmallTest
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());

        final String method = "PUT";
        builder.setHttpMethod(method);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("PUT");
    }

    @Test
    @SmallTest
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectToEchoBody(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call before the rewind, 1 after.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadRedirectAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectToEchoBody(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        dataProvider.assertClosed();
        callback.blockForDone();

        // 1 read call before the rewind, 1 after.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadWithBadLength() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor()) {
            @Override
            public long getLength() throws IOException {
                return 1;
            }

            @Override
            public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
                    throws IOException {
                byteBuffer.put("12".getBytes());
                uploadDataSink.onReadSucceeded(false);
            }
        };
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Read upload data length 2 exceeds expected length 1",
                callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadWithBadLengthBufferAligned() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor()) {
            @Override
            public long getLength() throws IOException {
                return 8191;
            }

            @Override
            public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
                    throws IOException {
                byteBuffer.put("0123456789abcdef".getBytes());
                uploadDataSink.onReadSucceeded(false);
            }
        };
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();
        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Read upload data length 8192 exceeds expected length 8191",
                callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadReadFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync read failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadLengthFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setLengthFailure();
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(0);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync length failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadReadFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Async read failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    /** This test uses a direct executor for upload, and non direct for callbacks */
    @Test
    @SmallTest
    public void testDirectExecutorUploadProhibitedByDefault() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        Executor myExecutor = new Executor() {

            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, myExecutor);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, myExecutor);
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(0);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Inline execution is prohibited for this request",
                callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    /** This test uses a direct executor for callbacks, and non direct for upload */
    @Test
    @SmallTest
    public void testDirectExecutorProhibitedByDefault() throws Exception {
        System.out.println("testing with " + mTestFramework.mCronetEngine);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        Executor myExecutor = new Executor() {

            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, myExecutor);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        callback.mError.printStackTrace();
        assertContains("Exception posting task to executor", callback.mError.getMessage());
        assertContains("Inline execution is prohibited for this request",
                callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
        dataProvider.assertClosed();
    }

    @Test
    @SmallTest
    public void testDirectExecutorAllowed() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAllowDirectExecutor(true);
        Executor myExecutor = new Executor() {

            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, myExecutor);
        UploadDataProvider dataProvider = UploadDataProviders.create("test".getBytes());
        builder.setUploadDataProvider(dataProvider, myExecutor);
        builder.addHeader("Content-Type", "useless/string");
        builder.allowDirectExecutor();
        builder.build().start();
        callback.blockForDone();

        if (callback.mOnErrorCalled) {
            throw callback.mError;
        }

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadReadFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.THROWN);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Thrown read failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadRewindFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectToEchoBody(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync rewind failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadRewindFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectToEchoBody(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Async rewind failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadRewindFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectToEchoBody(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.THROWN);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Thrown rewind failure", callback.mError.getCause().getMessage());
        assertThat(callback.mResponseInfo).isNull();
    }

    @Test
    @SmallTest
    public void testUploadChunked() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test hello".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertThat(dataProvider.getUploadedLength()).isEqualTo(-1);

        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call for one data chunk.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(callback.mResponseAsString).isEqualTo("test hello");
    }

    @Test
    @SmallTest
    public void testUploadChunkedLastReadZeroLengthBody() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        // Add 3 reads. The last read has a 0-length body.
        dataProvider.addRead("hello there".getBytes());
        dataProvider.addRead("!".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertThat(dataProvider.getUploadedLength()).isEqualTo(-1);

        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 2 read call for the first two data chunks, and 1 for final chunk.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(3);
        assertThat(callback.mResponseAsString).isEqualTo("hello there!");
    }

    // Test where an upload fails without ever initializing the
    // UploadDataStream, because it can't connect to the server.
    @Test
    @SmallTest
    public void testUploadFailsWithoutInitializingStream() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // The port for PTP will always refuse a TCP connection
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                "http://127.0.0.1:319", callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertNull(callback.mResponseInfo);
        if (mTestRule.testingJavaImpl()) {
            Throwable cause = callback.mError.getCause();
            assertTrue("Exception was: " + cause, cause instanceof ConnectException);
        } else {
            assertContains("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED",
                    callback.mError.getMessage());
        }
    }

    private void throwOrCancel(FailureType failureType, ResponseStep failureStep,
            boolean expectResponseInfo, boolean expectError) {
        if (Log.isLoggable("TESTING", Log.VERBOSE)) {
            Log.v("TESTING", "Testing " + failureType + " during " + failureStep);
        }
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setFailure(failureType, failureStep);
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getRedirectURL(), callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        // Wait for all posted tasks to be executed to ensure there is no unhandled exception.
        callback.shutdownExecutorAndWait();
        assertThat(callback.mRedirectCount).isEqualTo(1);
        if (failureType == FailureType.CANCEL_SYNC || failureType == FailureType.CANCEL_ASYNC) {
            assertResponseStepCanceled(callback);
        } else if (failureType == FailureType.THROW_SYNC) {
            assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        }
        assertTrue(urlRequest.isDone());
        assertThat(callback.mResponseInfo != null).isEqualTo(expectResponseInfo);
        assertThat(callback.mError != null).isEqualTo(expectError);
        assertThat(callback.mOnErrorCalled).isEqualTo(expectError);
        // When failureType is FailureType.CANCEL_ASYNC_WITHOUT_PAUSE and failureStep is
        // ResponseStep.ON_READ_COMPLETED, there might be an onSucceeded() task already posted. If
        // that's the case, onCanceled() will not be invoked. See crbug.com/657415.
        if (!(failureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE
                    && failureStep == ResponseStep.ON_READ_COMPLETED)) {
            assertThat(callback.mOnCanceledCalled)
                    .isEqualTo(failureType == FailureType.CANCEL_SYNC
                            || failureType == FailureType.CANCEL_ASYNC
                            || failureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE);
        }
    }

    @Test
    @SmallTest
    public void testFailures() throws Exception {
        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_READ_COMPLETED,
                true, true);
    }

    @Test
    @SmallTest
    public void testThrowOrCancelInOnSucceeded() {
        FailureType[] testTypes = new FailureType[] {
                FailureType.THROW_SYNC, FailureType.CANCEL_SYNC, FailureType.CANCEL_ASYNC};
        for (FailureType type : testTypes) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            callback.setFailure(type, ResponseStep.ON_SUCCEEDED);
            UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());
            UrlRequest urlRequest = builder.build();
            urlRequest.start();
            callback.blockForDone();
            // Wait for all posted tasks to be executed to ensure there is no unhandled exception.
            callback.shutdownExecutorAndWait();
            assertNull(callback.mError);
            assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
            assertTrue(urlRequest.isDone());
            assertNotNull(callback.mResponseInfo);
            assertFalse(callback.mOnErrorCalled);
            assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
            assertThat(callback.mResponseAsString).isEqualTo("GET");
        }
    }

    @Test
    @SmallTest
    public void testThrowOrCancelInOnFailed() {
        FailureType[] testTypes = new FailureType[] {
                FailureType.THROW_SYNC, FailureType.CANCEL_SYNC, FailureType.CANCEL_ASYNC};
        for (FailureType type : testTypes) {
            String url = NativeTestServer.getEchoBodyURL();
            // Shut down NativeTestServer so request will fail.
            NativeTestServer.shutdownNativeTestServer();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            callback.setFailure(type, ResponseStep.ON_FAILED);
            UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    url, callback, callback.getExecutor());
            UrlRequest urlRequest = builder.build();
            urlRequest.start();
            callback.blockForDone();
            // Wait for all posted tasks to be executed to ensure there is no unhandled exception.
            callback.shutdownExecutorAndWait();
            assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
            assertTrue(callback.mOnErrorCalled);
            assertNotNull(callback.mError);
            assertTrue(urlRequest.isDone());
            // Start NativeTestServer again to run the test for a second time.
            assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        }
    }

    @Test
    @SmallTest
    public void testThrowOrCancelInOnCanceled() {
        FailureType[] testTypes = new FailureType[] {
                FailureType.THROW_SYNC, FailureType.CANCEL_SYNC, FailureType.CANCEL_ASYNC};
        for (FailureType type : testTypes) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback() {
                @Override
                public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                    super.onResponseStarted(request, info);
                    request.cancel();
                }
            };
            callback.setFailure(type, ResponseStep.ON_CANCELED);
            UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());
            UrlRequest urlRequest = builder.build();
            urlRequest.start();
            callback.blockForDone();
            // Wait for all posted tasks to be executed to ensure there is no unhandled exception.
            callback.shutdownExecutorAndWait();
            assertResponseStepCanceled(callback);
            assertTrue(urlRequest.isDone());
            assertNotNull(callback.mResponseInfo);
            assertNull(callback.mError);
            assertTrue(callback.mOnCanceledCalled);
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // No destroyed callback for tests
    public void testExecutorShutdown() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());
        CronetUrlRequest urlRequest = (CronetUrlRequest) builder.build();
        urlRequest.start();
        callback.waitForNextStep();
        assertFalse(callback.isDone());
        assertFalse(urlRequest.isDone());

        final ConditionVariable requestDestroyed = new ConditionVariable(false);
        urlRequest.setOnDestroyedCallbackForTesting(new Runnable() {
            @Override
            public void run() {
                requestDestroyed.open();
            }
        });

        // Shutdown the executor, so posting the task will throw an exception.
        callback.shutdownExecutor();
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.read(readBuffer);
        // Callback will never be called again because executor is shutdown,
        // but request will be destroyed from network thread.
        requestDestroyed.block();

        assertFalse(callback.isDone());
        assertTrue(urlRequest.isDone());
    }

    @Test
    @SmallTest
    public void testUploadExecutorShutdown() throws Exception {
        class HangingUploadDataProvider extends UploadDataProvider {
            UploadDataSink mUploadDataSink;
            ByteBuffer mByteBuffer;
            ConditionVariable mReadCalled = new ConditionVariable(false);

            @Override
            public long getLength() {
                return 69;
            }

            @Override
            public void read(final UploadDataSink uploadDataSink,
                             final ByteBuffer byteBuffer) {
                mUploadDataSink = uploadDataSink;
                mByteBuffer = byteBuffer;
                mReadCalled.open();
            }

            @Override
            public void rewind(final UploadDataSink uploadDataSink) {
            }
        }

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        ExecutorService uploadExecutor = Executors.newSingleThreadExecutor();
        HangingUploadDataProvider dataProvider = new HangingUploadDataProvider();
        builder.setUploadDataProvider(dataProvider, uploadExecutor);
        builder.addHeader("Content-Type", "useless/string");
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        // Wait for read to be called on executor.
        dataProvider.mReadCalled.block();
        // Shutdown the executor, so posting next task will throw an exception.
        uploadExecutor.shutdown();
        // Continue the upload.
        dataProvider.mByteBuffer.putInt(42);
        dataProvider.mUploadDataSink.onReadSucceeded(false);
        // Callback.onFailed will be called on request executor even though upload
        // executor is shutdown.
        callback.blockForDone();
        assertTrue(callback.isDone());
        assertTrue(callback.mOnErrorCalled);
        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertTrue(urlRequest.isDone());
    }

    /**
     * A TestUrlRequestCallback that shuts down executor upon receiving onSucceeded callback.
     */
    private static class QuitOnSuccessCallback extends TestUrlRequestCallback {
        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            // Stop accepting new tasks.
            shutdownExecutor();
            super.onSucceeded(request, info);
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // No adapter to destroy in pure java
    // Regression test for crbug.com/564946.
    public void testDestroyUploadDataStreamAdapterOnSucceededCallback() throws Exception {
        TestUrlRequestCallback callback = new QuitOnSuccessCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoBodyURL(), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        CronetUrlRequest request = (CronetUrlRequest) builder.build();
        final ConditionVariable uploadDataStreamAdapterDestroyed = new ConditionVariable();
        request.setOnDestroyedUploadCallbackForTesting(new Runnable() {
            @Override
            public void run() {
                uploadDataStreamAdapterDestroyed.open();
            }
        });

        request.start();
        uploadDataStreamAdapterDestroyed.block();
        callback.blockForDone();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEmpty();
    }

    /*
     * Verifies error codes are passed through correctly.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java impl doesn't support MockUrlRequestJobFactory
    public void testErrorCodes() throws Exception {
        checkSpecificErrorCode(
                -105, NetworkException.ERROR_HOSTNAME_NOT_RESOLVED, "NAME_NOT_RESOLVED", false);
        checkSpecificErrorCode(
                -106, NetworkException.ERROR_INTERNET_DISCONNECTED, "INTERNET_DISCONNECTED", false);
        checkSpecificErrorCode(
                -21, NetworkException.ERROR_NETWORK_CHANGED, "NETWORK_CHANGED", true);
        checkSpecificErrorCode(
                -100, NetworkException.ERROR_CONNECTION_CLOSED, "CONNECTION_CLOSED", true);
        checkSpecificErrorCode(
                -102, NetworkException.ERROR_CONNECTION_REFUSED, "CONNECTION_REFUSED", false);
        checkSpecificErrorCode(
                -101, NetworkException.ERROR_CONNECTION_RESET, "CONNECTION_RESET", true);
        checkSpecificErrorCode(
                -118, NetworkException.ERROR_CONNECTION_TIMED_OUT, "CONNECTION_TIMED_OUT", true);
        checkSpecificErrorCode(-7, NetworkException.ERROR_TIMED_OUT, "TIMED_OUT", true);
        checkSpecificErrorCode(
                -109, NetworkException.ERROR_ADDRESS_UNREACHABLE, "ADDRESS_UNREACHABLE", false);
        checkSpecificErrorCode(-2, NetworkException.ERROR_OTHER, "FAILED", false);
    }

    /*
     * Verifies no cookies are saved or sent by default.
     */
    @Test
    @SmallTest
    public void testCookiesArentSavedOrSent() throws Exception {
        // Make a request to a url that sets the cookie
        String url = NativeTestServer.getFileURL("/set_cookie.html");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("Set-Cookie", Arrays.asList("A=B"));

        // Make a request that check that cookie header isn't sent.
        String headerName = "Cookie";
        String url2 = NativeTestServer.getEchoHeaderURL(headerName);
        TestUrlRequestCallback callback2 = startAndWaitForComplete(url2);
        assertThat(callback2.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback2.mResponseAsString).isEqualTo("Header not found. :(");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testQuicErrorCode() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.START, NetError.ERR_QUIC_PROTOCOL_ERROR));
        assertNull(callback.mResponseInfo);
        assertThat(callback.mError).isInstanceOf(QuicException.class);
        QuicException quicException = (QuicException) callback.mError;
        // 1 is QUIC_INTERNAL_ERROR
        assertThat(quicException.getQuicDetailedErrorCode()).isEqualTo(1);
        assertThat(quicException.getErrorCode())
                .isEqualTo(NetworkException.ERROR_QUIC_PROTOCOL_FAILED);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testQuicErrorCodeForNetworkChanged() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.START, NetError.ERR_NETWORK_CHANGED));
        assertNull(callback.mResponseInfo);
        assertThat(callback.mError).isInstanceOf(QuicException.class);
        QuicException quicException = (QuicException) callback.mError;
        // QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK(83) is set in
        // URLRequestFailedJob::PopulateNetErrorDetails for this test.
        final int quicErrorCode = 83;
        assertThat(quicException.getQuicDetailedErrorCode()).isEqualTo(quicErrorCode);
        assertThat(quicException.getErrorCode()).isEqualTo(NetworkException.ERROR_NETWORK_CHANGED);
    }

    /**
     * Tests that legacy onFailed callback is invoked with UrlRequestException if there
     * is no onFailed callback implementation that takes CronetException.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLegacyOnFailedCallback() throws Exception {
        final int netError = -123;
        final AtomicBoolean failedExpectation = new AtomicBoolean();
        final ConditionVariable done = new ConditionVariable();
        UrlRequest.Callback callback = new UrlRequest.Callback() {
            @Override
            public void onRedirectReceived(
                    UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onReadCompleted(
                    UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                failedExpectation.set(true);
                fail();
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                assertThat(error).isInstanceOf(NetworkException.class);
                assertThat(((NetworkException) error).getCronetInternalErrorCode())
                        .isEqualTo(netError);
                failedExpectation.set(
                        ((NetworkException) error).getCronetInternalErrorCode() != netError);
                done.open();
            }

            @Override
            public void onCanceled(UrlRequest request, UrlResponseInfo info) {
                failedExpectation.set(true);
                fail();
            }
        };

        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                MockUrlRequestJobFactory.getMockUrlWithFailure(FailurePhase.START, netError),
                callback, Executors.newSingleThreadExecutor());
        final UrlRequest urlRequest = builder.build();
        urlRequest.start();
        done.block();
        // Check that onFailed is called.
        assertFalse(failedExpectation.get());
    }

    private void checkSpecificErrorCode(int netError, int errorCode, String name,
            boolean immediatelyRetryable) throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlWithFailure(FailurePhase.START, netError));
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(netError);
        assertThat(((NetworkException) callback.mError).getErrorCode()).isEqualTo(errorCode);
        assertThat(((NetworkException) callback.mError).immediatelyRetryable())
                .isEqualTo(immediatelyRetryable);
        assertContains(
                "Exception in CronetUrlRequest: net::ERR_" + name, callback.mError.getMessage());
        assertThat(callback.mRedirectCount).isEqualTo(0);
        assertTrue(callback.mOnErrorCalled);
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }

    // Returns the contents of byteBuffer, from its position() to its limit(),
    // as a String. Does not modify byteBuffer's position().
    private String bufferContentsToString(ByteBuffer byteBuffer, int start, int end) {
        // Use a duplicate to avoid modifying byteBuffer.
        ByteBuffer duplicate = byteBuffer.duplicate();
        duplicate.position(start);
        duplicate.limit(end);
        byte[] contents = new byte[duplicate.remaining()];
        duplicate.get(contents);
        return new String(contents);
    }

    private void assertResponseStepCanceled(TestUrlRequestCallback callback) {
        if (callback.mResponseStep == ResponseStep.ON_FAILED && callback.mError != null) {
            throw new Error(
                    "Unexpected response state: " + ResponseStep.ON_FAILED, callback.mError);
        }
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_CANCELED);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    // Used for Android's NetworkSecurityPolicy added in Nougat
    public void testCleartextTrafficBlocked() throws Exception {
        final int cleartextNotPermitted = -29;
        // This hostname needs to match the one in network_security_config.xml and the one used
        // by QuicTestServer.
        // https requests to it are tested in QuicTest, so this checks that we're only blocking
        // cleartext.
        final String url = "http://example.com/simple.txt";
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(cleartextNotPermitted);
    }

    @Test
    @SmallTest
    /**
     * Open many connections and cancel them right away. This test verifies all internal
     * sockets and other Closeables are properly closed. See crbug.com/726193.
     */
    public void testGzipCancel() throws Exception {
        String url = NativeTestServer.getFileURL("/gzipped.html");
        for (int i = 0; i < 100; i++) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            callback.setAutoAdvance(false);
            UrlRequest urlRequest =
                    mTestFramework.mCronetEngine
                            .newUrlRequestBuilder(url, callback, callback.getExecutor())
                            .build();
            urlRequest.start();
            urlRequest.cancel();
            // If the test blocks until each UrlRequest finishes before starting the next UrlRequest
            // then it never catches the leak. If it starts all UrlRequests and then blocks until
            // all UrlRequests finish, it only catches the leak ~10% of the time. In its current
            // form it appears to catch the leak ~70% of the time.
            // Catching the leak may require a lot of busy threads so that the cancel() happens
            // before the UrlRequest has made much progress (and set mCurrentUrlConnection and
            // mResponseChannel).  This may be why blocking until each UrlRequest finishes doesn't
            // catch the leak.
            // The other quirk of this is that from teardown(), JavaCronetEngine.shutdown() is
            // called which calls ExecutorService.shutdown() which doesn't wait for the thread to
            // finish running tasks, and then teardown() calls GC looking for leaks. One possible
            // modification would be to expose the ExecutorService and then have tests call
            // awaitTermination() but this would complicate things, and adding a 1s sleep() to
            // allow the ExecutorService to terminate did not increase the chances of catching the
            // leak.
        }
    }

    @Test
    @SmallTest
    @RequiresMinApi(8) // JavaUrlRequest fixed in API level 8: crrev.com/499303
    /** Do a HEAD request and get back a 404. */
    public void test404Head() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getFileURL("/notfound.html"), callback, callback.getExecutor());
        builder.setHttpMethod("HEAD").build().start();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    @RequiresMinApi(9) // Tagging support added in API level 9: crrev.com/c/chromium/src/+/930086
    @RequiresMinAndroidApi(Build.VERSION_CODES.M) // crbug/1301957
    public void testTagging() throws Exception {
        if (!CronetTestUtil.nativeCanGetTaggedBytes()) {
            Log.i(TAG, "Skipping test - GetTaggedBytes unsupported.");
            return;
        }
        String url = NativeTestServer.getEchoMethodURL();

        // Test untagged requests are given tag 0.
        int tag = 0;
        long priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test explicit tagging.
        tag = 0x12345678;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestUrlRequestCallback();
        builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsTag(tag));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test a different tag value to make sure reused connections are retagged.
        tag = 0x87654321;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestUrlRequestCallback();
        builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsTag(tag));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test tagging with our UID.
        tag = 0;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestUrlRequestCallback();
        builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsUid(Process.myUid()));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);
    }

    @Test
    @SmallTest
    /**
     * Initiate many requests concurrently to make sure neither Cronet implementation crashes.
     * Regression test for https://crbug.com/844031.
     */
    public void testManyRequests() throws Exception {
        String url = NativeTestServer.getMultiRedirectURL();
        final int numRequests = 2000;
        TestUrlRequestCallback callbacks[] = new TestUrlRequestCallback[numRequests];
        UrlRequest requests[] = new UrlRequest[numRequests];
        for (int i = 0; i < numRequests; i++) {
            // Share the first callback's executor to avoid creating too many single-threaded
            // executors and hence too many threads.
            if (i == 0) {
                callbacks[i] = new TestUrlRequestCallback();
            } else {
                callbacks[i] = new TestUrlRequestCallback(callbacks[0].getExecutor());
            }
            UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    url, callbacks[i], callbacks[i].getExecutor());
            requests[i] = builder.build();
        }
        for (UrlRequest request : requests) {
            request.start();
        }
        for (UrlRequest request : requests) {
            request.cancel();
        }
        for (TestUrlRequestCallback callback : callbacks) {
            callback.blockForDone();
        }
    }

    @Test
    @SmallTest
    public void testSetIdempotency() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoMethodURL(), callback, callback.getExecutor());
        assertThat(builder).isEqualTo(
                builder.setIdempotency(ExperimentalUrlRequest.Builder.IDEMPOTENT));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("POST");
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        // Return connection migration disable load flag value.
        int getConnectionMigrationDisableLoadFlag();
    }
}
