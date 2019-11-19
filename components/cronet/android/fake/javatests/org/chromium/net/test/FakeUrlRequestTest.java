// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;

import static org.chromium.net.CronetTestRule.assertContains;
import static org.chromium.net.TestUrlRequestCallback.ResponseStep.ON_CANCELED;

import android.content.Context;
import android.os.ConditionVariable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.net.CronetEngine;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.TestUploadDataProvider;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataProviders;
import org.chromium.net.UploadDataSink;
import org.chromium.net.UrlRequest.Status;
import org.chromium.net.UrlRequest.StatusListener;

import java.io.IOException;
import java.net.URI;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test functionality of FakeUrlRequest.
 */
@RunWith(AndroidJUnit4.class)
public class FakeUrlRequestTest {
    private CronetEngine mFakeCronetEngine;
    private FakeCronetController mFakeCronetController;

    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    private static void checkStatus(FakeUrlRequest request, final int expectedStatus) {
        ConditionVariable foundStatus = new ConditionVariable();
        request.getStatus(new StatusListener() {
            @Override
            public void onStatus(int status) {
                assertEquals(expectedStatus, status);
                foundStatus.open();
            }
        });
        foundStatus.block();
    }

    private class EchoBodyResponseMatcher implements ResponseMatcher {
        private final String mUrl;

        EchoBodyResponseMatcher(String url) {
            mUrl = url;
        }

        EchoBodyResponseMatcher() {
            this(null);
        }

        @Override
        public FakeUrlResponse getMatchingResponse(String url, String httpMethod,
                List<Map.Entry<String, String>> headers, byte[] body) {
            if (mUrl == null || mUrl.equals(url)) {
                return new FakeUrlResponse.Builder().setResponseBody(body).build();
            }
            return null;
        }
    }

    @Before
    public void setUp() {
        mFakeCronetController = new FakeCronetController();
        mFakeCronetEngine = mFakeCronetController.newFakeCronetEngineBuilder(getContext()).build();
    }

    @After
    public void tearDown() {
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testDefaultResponse() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody(responseText.getBytes()).build();
        String url = "www.response.com";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify correct callback methods called and correct response returned.
        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(1)).onReadCompleted(any(), any(), any());
        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertEquals(callback.mResponseAsString, responseText);
    }

    @Test
    @SmallTest
    public void testBuilderChecks() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        try {
            mFakeCronetEngine.newUrlRequestBuilder(null, callback, callback.getExecutor());
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
        try {
            mFakeCronetEngine.newUrlRequestBuilder("url", null, callback.getExecutor());
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Callback is required.", e.getMessage());
        }
        try {
            mFakeCronetEngine.newUrlRequestBuilder("url", callback, null);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Executor is required.", e.getMessage());
        }
        // Verify successful creation doesn't throw.
        mFakeCronetEngine.newUrlRequestBuilder("url", callback, callback.getExecutor());
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenNullFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        // Check exception thrown for null method.
        try {
            request.setHttpMethod(null);
            fail("Method not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Method is required.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenInvalidFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();

        // Check exception thrown for invalid method.
        String method = "BADMETHOD";
        try {
            request.setHttpMethod(method);
            fail("Method not checked for validity");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid http method: " + method, e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testSetHttpMethodSetsMethodToCorrectMethod() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        String testMethod = "PUT";
        // Use an atomic because it is set in an inner class. We do not actually need atomic for a
        // multi-threaded operation here.
        AtomicBoolean foundMethod = new AtomicBoolean();

        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(String url, String httpMethod,
                    List<Map.Entry<String, String>> headers, byte[] body) {
                assertEquals(testMethod, httpMethod);
                foundMethod.set(true);
                // It doesn't matter if a response is actually returned.
                return null;
            }
        });

        // Check no exception for correct method.
        request.setHttpMethod(testMethod);

        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertTrue(foundMethod.get());
    }

    @Test
    @SmallTest
    public void testAddHeader() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();
        String headerKey = "HEADERNAME";
        String headerValue = "HEADERVALUE";
        request.addHeader(headerKey, headerValue);
        // Use an atomic because it is set in an inner class. We do not actually need atomic for a
        // multi-threaded operation here.
        AtomicBoolean foundEntry = new AtomicBoolean();
        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(String url, String httpMethod,
                    List<Map.Entry<String, String>> headers, byte[] body) {
                assertEquals(1, headers.size());
                assertEquals(headerKey, headers.get(0).getKey());
                assertEquals(headerValue, headers.get(0).getValue());
                foundEntry.set(true);
                // It doesn't matter if a response is actually returned.
                return null;
            }
        });
        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertTrue(foundEntry.get());
    }

    @Test
    @SmallTest
    public void testRequestDoesNotStartWhenEngineShutDown() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();

        mFakeCronetEngine.shutdown();
        try {
            request.start();
            fail("Request should check that the CronetEngine is not shutdown before starting.");
        } catch (IllegalStateException e) {
            assertEquals("This request's CronetEngine is already shutdown.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testRequestStopsWhenCanceled() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();
        callback.setAutoAdvance(false);
        request.start();
        callback.waitForNextStep();
        request.cancel();

        callback.blockForDone();

        Mockito.verify(callback, times(1)).onCanceled(any(), any());
        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(0)).onReadCompleted(any(), any(), any());
        assertEquals(callback.mResponseStep, ON_CANCELED);
    }

    @Test
    @SmallTest
    public void testRecievedByteCountInUrlResponseInfoIsEqualToResponseLength() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody(responseText.getBytes()).build();
        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(responseText.length(), callback.mResponseInfo.getReceivedByteCount());
    }

    @Test
    @SmallTest
    public void testRedirectResponse() {
        // Setup the basic response.
        String responseText = "response text";
        String redirectLocation = "/redirect_location";
        FakeUrlResponse response = new FakeUrlResponse.Builder()
                                           .setResponseBody(responseText.getBytes())
                                           .addHeader("location", redirectLocation)
                                           .setHttpStatusCode(300)
                                           .build();

        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        String redirectText = "redirect text";
        FakeUrlResponse redirectToResponse =
                new FakeUrlResponse.Builder().setResponseBody(redirectText.getBytes()).build();
        String redirectUrl = URI.create(url).resolve(redirectLocation).toString();

        mFakeCronetController.addResponseForUrl(redirectToResponse, redirectUrl);

        // Run the request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertTrue(Objects.equals(callback.mResponseAsString, redirectText));
    }

    @Test
    @SmallTest
    public void testRedirectResponseWithNoHeaderFails() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response = new FakeUrlResponse.Builder()
                                           .setResponseBody(responseText.getBytes())
                                           .setHttpStatusCode(300)
                                           .build();

        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertEquals(TestUrlRequestCallback.ResponseStep.ON_FAILED, callback.mResponseStep);
    }

    @Test
    @SmallTest
    public void testResponseLongerThanBuffer() {
        // Build a long response string that is 3x the buffer size.
        final int bufferStringLengthMultiplier = 3;
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        String longResponseString =
                new String(new char[callback.mReadBufferSize * bufferStringLengthMultiplier]);

        String longResponseUrl = "https://www.longResponseUrl.com";

        FakeUrlResponse reallyLongResponse = new FakeUrlResponse.Builder()
                                                     .setResponseBody(longResponseString.getBytes())
                                                     .build();
        mFakeCronetController.addResponseForUrl(reallyLongResponse, longResponseUrl);

        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(longResponseUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(bufferStringLengthMultiplier))
                .onReadCompleted(any(), any(), any());
        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertTrue(Objects.equals(callback.mResponseAsString, longResponseString));
    }

    @Test
    @SmallTest
    public void testStatusInvalidBeforeStart() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();

        checkStatus(request, Status.INVALID);
        request.start();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusIdleWhenWaitingForRead() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();
        request.start();
        checkStatus(request, Status.IDLE);
        callback.setAutoAdvance(true);
        callback.startNextRead(request);
        callback.blockForDone();
    }

    @DisabledTest(message = "crbug.com/994722")
    @Test
    @SmallTest
    public void testStatusIdleWhenWaitingForRedirect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String initialURL = "initialURL";
        String secondURL = "secondURL";
        mFakeCronetController.addRedirectResponse(secondURL, initialURL);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(initialURL, callback, callback.getExecutor())
                        .build();

        request.start();
        checkStatus(request, Status.IDLE);
        callback.setAutoAdvance(true);
        request.followRedirect();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusInvalidWhenDone() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();
        checkStatus(request, Status.INVALID);
    }

    @Test
    @SmallTest
    public void testIsDoneWhenComplete() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder("", callback, callback.getExecutor())
                                         .build();

        request.start();
        callback.blockForDone();

        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertTrue(request.isDone());
    }

    @Test
    @SmallTest
    public void testSetUploadDataProviderAfterStart() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder("", callback, callback.getExecutor())
                                         .addHeader("Content-Type", "useless/string")
                                         .build();
        String body = "body";
        request.setUploadDataProvider(
                UploadDataProviders.create(body.getBytes()), callback.getExecutor());
        request.start();
        // Must wait for the request to prevent a race in the State since it is reported in the
        // error.
        callback.blockForDone();

        try {
            request.setUploadDataProvider(
                    UploadDataProviders.create(body.getBytes()), callback.getExecutor());
            fail("UploadDataProvider cannot be changed after request has started");
        } catch (IllegalStateException e) {
            assertEquals("Request is already started. State is: 7", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testUrlChainIsCorrectForSuccessRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        List<String> expectedUrlChain = new ArrayList<>();
        expectedUrlChain.add(testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedUrlChain, callback.mResponseInfo.getUrlChain());
    }

    @Test
    @SmallTest
    public void testUrlChainIsCorrectForRedirectRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl1 = "TEST_URL1";
        String testUrl2 = "TEST_URL2";
        mFakeCronetController.addRedirectResponse(testUrl2, testUrl1);
        List<String> expectedUrlChain = new ArrayList<>();
        expectedUrlChain.add(testUrl1);
        expectedUrlChain.add(testUrl2);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl1, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedUrlChain, callback.mResponseInfo.getUrlChain());
    }

    @Test
    @SmallTest
    public void testResponseCodeCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        int expectedResponseCode = 208;
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setHttpStatusCode(expectedResponseCode).build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedResponseCode, callback.mResponseInfo.getHttpStatusCode());
    }

    @Test
    @SmallTest
    public void testResponseTextCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        int expectedResponseCode = 208;
        String expectedResponseText = "Already Reported";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setHttpStatusCode(expectedResponseCode).build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedResponseText, callback.mResponseInfo.getHttpStatusText());
    }

    @Test
    @SmallTest
    public void testResponseWasCachedCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        boolean expectedWasCached = true;
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setWasCached(expectedWasCached).build(), testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedWasCached, callback.mResponseInfo.wasCached());
    }

    @Test
    @SmallTest
    public void testResponseNegotiatedProtocolCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        String expectedNegotiatedProtocol = "TEST_NEGOTIATED_PROTOCOL";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setNegotiatedProtocol(expectedNegotiatedProtocol)
                        .build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedNegotiatedProtocol, callback.mResponseInfo.getNegotiatedProtocol());
    }

    @Test
    @SmallTest
    public void testResponseProxyServerCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        String expectedProxyServer = "TEST_PROXY_SERVER";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setProxyServer(expectedProxyServer).build(), testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedProxyServer, callback.mResponseInfo.getProxyServer());
    }

    @Test
    @SmallTest
    public void testDirectExecutorDisabledByDefault() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAllowDirectExecutor(true);
        Executor myExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine.newUrlRequestBuilder("url", callback, myExecutor)
                        .build();

        request.start();
        Mockito.verify(callback).onFailed(any(), any(), any());
        // Checks that the exception from {@link DirectPreventingExecutor} was successfully returned
        // to the callabck in the onFailed method.
        assertTrue(callback.mError.getCause() instanceof InlineExecutionProhibitedException);
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflow() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        // Make the buffer size small so there are lots of calls to read().
        callback.mReadBufferSize = 1;
        String testUrl = "TEST_URL";
        int responseLength = 1024;
        byte[] byteArray = new byte[responseLength];
        Arrays.fill(byteArray, (byte) 1);
        String longResponseString = new String(byteArray);
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setResponseBody(longResponseString.getBytes())
                        .build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .allowDirectExecutor()
                        .build();
        request.start();
        callback.blockForDone();
        assertEquals(longResponseString, callback.mResponseAsString);
        Mockito.verify(callback, times(responseLength)).onReadCompleted(any(), any(), any());
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflowWithDirectExecutor() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAllowDirectExecutor(true);
        // Make the buffer size small so there are lots of calls to read().
        callback.mReadBufferSize = 1;
        String testUrl = "TEST_URL";
        int responseLength = 1024;
        byte[] byteArray = new byte[responseLength];
        Arrays.fill(byteArray, (byte) 1);
        String longResponseString = new String(byteArray);
        Executor myExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setResponseBody(longResponseString.getBytes())
                        .build(),
                testUrl);
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder(testUrl, callback, myExecutor)
                                         .allowDirectExecutor()
                                         .build();
        request.start();
        callback.blockForDone();
        assertEquals(longResponseString, callback.mResponseAsString);
        Mockito.verify(callback, times(responseLength)).onReadCompleted(any(), any(), any());
    }

    @Test
    @SmallTest
    public void testDoubleReadFails() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAutoAdvance(false);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        request.start();
        callback.startNextRead(request);
        try {
            callback.startNextRead(request);
            fail("Double read() should be disallowed.");
        } catch (IllegalStateException e) {
            assertEquals("Invalid state transition - expected 4 but was 7", e.getMessage());
        }
    }

    @DisabledTest(message = "crbug.com/994722")
    @Test
    @SmallTest
    public void testReadWhileRedirectingFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        mFakeCronetController.addRedirectResponse("location", url);
        request.start();
        try {
            callback.startNextRead(request);
            fail("Read should be disallowed while waiting for redirect.");
        } catch (IllegalStateException e) {
            assertEquals("Invalid state transition - expected 4 but was 3", e.getMessage());
        }
        callback.setAutoAdvance(true);
        request.followRedirect();
        callback.blockForDone();
    }

    @DisabledTest(message = "crbug.com/994722")
    @Test
    @SmallTest
    public void testShuttingDownCronetEngineWithActiveRequestFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();

        request.start();

        try {
            mFakeCronetEngine.shutdown();
            fail("Shutdown not checked for active requests.");
        } catch (IllegalStateException e) {
            assertEquals("Cannot shutdown with active requests.", e.getMessage());
        }
        callback.setAutoAdvance(true);
        callback.startNextRead(request);
        callback.blockForDone();
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testDefaultResponseIs404() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();

        request.start();
        callback.blockForDone();

        assertEquals(404, callback.mResponseInfo.getHttpStatusCode());
    }

    @Test
    @SmallTest
    public void testUploadSetDataProviderChecksForNullUploadDataProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        try {
            builder.setUploadDataProvider(null, callback.getExecutor());
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Invalid UploadDataProvider.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testUploadSetDataProviderChecksForContentTypeHeader() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        try {
            builder.build().start();
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Requests with upload data must have a Content-Type.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testUploadWithEmptyBody() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertNotNull(callback.mResponseInfo);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("", callback.mResponseAsString);
        dataProvider.assertClosed();
    }

    @Test
    @SmallTest
    public void testUploadSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        String body = "test";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        dataProvider.addRead(body.getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(4, dataProvider.getUploadedLength());
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadSyncReadWrongState() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        String body = "test";
        callback.setAutoAdvance(false);
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());

        // Add a redirect response so the request keeps the UploadDataProvider open while waiting
        // to follow the redirect.
        mFakeCronetController.addRedirectResponse("newUrl", url);
        dataProvider.addRead(body.getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        FakeUrlRequest request = (FakeUrlRequest) builder.build();
        request.start();
        callback.waitForNextStep();
        try {
            request.mFakeDataSink.onReadSucceeded(false);
            fail("Cannot read before upload has started");
        } catch (IllegalStateException e) {
            assertEquals("onReadSucceeded() called when not awaiting a read result; in state: 2",
                    e.getMessage());
        }
        request.cancel();
    }

    @Test
    @SmallTest
    public void testUploadSyncRewindWrongState() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        String body = "test";
        callback.setAutoAdvance(false);
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());

        // Add a redirect response so the request keeps the UploadDataProvider open while waiting
        // to follow the redirect.
        mFakeCronetController.addRedirectResponse("newUrl", url);
        dataProvider.addRead(body.getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        FakeUrlRequest request = (FakeUrlRequest) builder.build();
        request.start();
        callback.waitForNextStep();
        try {
            request.mFakeDataSink.onRewindSucceeded();
            fail("Cannot rewind before upload has started");
        } catch (IllegalStateException e) {
            assertEquals("onRewindSucceeded() called when not awaiting a rewind; in state: 2",
                    e.getMessage());
        }
        request.cancel();
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(16, dataProvider.getUploadedLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
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

        assertEquals(16, dataProvider.getUploadedLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadChangesDefaultMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(String url, String httpMethod,
                    List<Map.Entry<String, String>> headers, byte[] body) {
                return new FakeUrlResponse.Builder().setResponseBody(httpMethod.getBytes()).build();
            }
        });
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("POST", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(String url, String httpMethod,
                    List<Map.Entry<String, String>> headers, byte[] body) {
                return new FakeUrlResponse.Builder().setResponseBody(httpMethod.getBytes()).build();
            }
        });
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

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("PUT", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadRedirectAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadWithBadLength() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

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
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadWithBadLengthBufferAligned() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

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
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadLengthFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

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

        assertEquals(0, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync length failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadReadFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(
                /* readFailIndex= */ 0, TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadReadFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(
                /* readFailIndex= */ 0, TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Async read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadReadFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(/* readFailIndex= */ 0, TestUploadDataProvider.FailMode.THROWN);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Thrown read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
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
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, myExecutor);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, myExecutor);
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(0, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Inline execution is prohibited for this request",
                callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    /** This test uses a direct executor for callbacks, and non direct for upload */
    @Test
    @SmallTest
    public void testDirectExecutorProhibitedByDefault() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        Executor myExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, myExecutor);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertContains("Exception posting task to executor", callback.mError.getMessage());
        assertContains("Inline execution is prohibited for this request",
                callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
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
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        UploadDataProvider dataProvider = UploadDataProviders.create("test".getBytes());
        builder.setUploadDataProvider(dataProvider, myExecutor);
        builder.addHeader("Content-Type", "useless/string");
        builder.allowDirectExecutor();
        builder.build().start();
        callback.blockForDone();

        if (callback.mOnErrorCalled) {
            throw callback.mError;
        }

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadRewindFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Sync rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadRewindFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Async rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadRewindFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.THROWN);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertContains("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertContains("Thrown rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @Test
    @SmallTest
    public void testUploadChunked() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test hello".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getUploadedLength());

        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call for one data chunk.
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals("test hello", callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testUploadChunkedLastReadZeroLengthBody() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder) mFakeCronetEngine.newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        // Add 3 reads. The last read has a 0-length body.
        dataProvider.addRead("hello there".getBytes());
        dataProvider.addRead("!".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getUploadedLength());

        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 2 read call for the first two data chunks, and 1 for final chunk.
        assertEquals(3, dataProvider.getNumReadCalls());
        assertEquals("hello there!", callback.mResponseAsString);
    }
}
