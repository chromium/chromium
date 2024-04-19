// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.ConditionVariable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.common.collect.Maps;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.TestUploadDataProvider;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequest.Status;
import org.chromium.net.UrlRequest.StatusListener;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.apihelpers.UploadDataProviders;

import java.io.IOException;
import java.net.URI;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/** Test functionality of FakeUrlRequest. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class FakeUrlRequestTest {
    private CronetEngine mFakeCronetEngine;
    private FakeCronetController mFakeCronetController;

    private static void checkStatus(FakeUrlRequest request, final int expectedStatus) {
        ConditionVariable foundStatus = new ConditionVariable();
        request.getStatus(
                new StatusListener() {
                    @Override
                    public void onStatus(int status) {
                        assertThat(status).isEqualTo(expectedStatus);
                        foundStatus.open();
                    }
                });
        foundStatus.block();
    }

    // Asserts that read happens a specified number of times
    private static void assertReadCalled(
            int numberOfTimes, TestUrlRequestCallback callback, UrlRequest request) {
        for (int i = 1; i <= numberOfTimes; i++) {
            callback.startNextRead(request);
            callback.waitForNextStep();
            assertWithMessage(
                            "Expected read to happen "
                                    + numberOfTimes
                                    + " times but got "
                                    + i
                                    + " times.")
                    .that(callback.mResponseStep)
                    .isEqualTo(ResponseStep.ON_READ_COMPLETED);
        }
    }

    private static class EchoBodyResponseMatcher implements ResponseMatcher {
        private final String mUrl;

        EchoBodyResponseMatcher(String url) {
            mUrl = url;
        }

        EchoBodyResponseMatcher() {
            this(null);
        }

        @Override
        public FakeUrlResponse getMatchingResponse(
                String url,
                String httpMethod,
                List<Map.Entry<String, String>> headers,
                byte[] body) {
            if (mUrl == null || mUrl.equals(url)) {
                return new FakeUrlResponse.Builder().setResponseBody(body).build();
            }
            return null;
        }
    }

    @Before
    public void setUp() {
        mFakeCronetController = new FakeCronetController();
        mFakeCronetEngine =
                mFakeCronetController
                        .newFakeCronetEngineBuilder(ApplicationProvider.getApplicationContext())
                        .build();
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
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.waitForNextStep();

        // Verify correct callback methods called and correct response returned.
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        callback.startNextRead(request);
        callback.waitForNextStep();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);
        callback.startNextRead(request);
        callback.waitForNextStep();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        callback.blockForDone();

        assertThat(responseText).isEqualTo(callback.mResponseAsString);
    }

    @Test
    @SmallTest
    public void testBuilderChecks() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        NullPointerException e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                mFakeCronetEngine.newUrlRequestBuilder(
                                        null, callback, callback.getExecutor()));
        assertThat(e).hasMessageThat().isEqualTo("URL is required.");

        e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                mFakeCronetEngine.newUrlRequestBuilder(
                                        "url", null, callback.getExecutor()));
        assertThat(e).hasMessageThat().isEqualTo("Callback is required.");

        e =
                assertThrows(
                        NullPointerException.class,
                        () -> mFakeCronetEngine.newUrlRequestBuilder("url", callback, null));
        assertThat(e).hasMessageThat().isEqualTo("Executor is required.");
        // Verify successful creation doesn't throw.
        mFakeCronetEngine.newUrlRequestBuilder("url", callback, callback.getExecutor());
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenNullFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mFakeCronetEngine.newUrlRequestBuilder("url", callback, callback.getExecutor());
        NullPointerException e =
                assertThrows(NullPointerException.class, () -> builder.setHttpMethod(null).build());
        assertThat(e).hasMessageThat().isEqualTo("Method is required.");
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenInvalidFails() {
        String method = "BADMETHOD";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mFakeCronetEngine.newUrlRequestBuilder("url", callback, callback.getExecutor());
        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class,
                        () -> builder.setHttpMethod(method).build());
        assertThat(e).hasMessageThat().isEqualTo("Invalid http method: " + method);
    }

    @Test
    @SmallTest
    public void testSetHttpMethodSetsMethodToCorrectMethod() {
        String testMethod = "PUT";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("url", callback, callback.getExecutor())
                                .setHttpMethod(testMethod)
                                .build();

        // Use an atomic because it is set in an inner class. We do not actually need atomic for a
        // multi-threaded operation here.
        AtomicBoolean foundMethod = new AtomicBoolean();

        mFakeCronetController.addResponseMatcher(
                new ResponseMatcher() {
                    @Override
                    public FakeUrlResponse getMatchingResponse(
                            String url,
                            String httpMethod,
                            List<Map.Entry<String, String>> headers,
                            byte[] body) {
                        assertThat(httpMethod).isEqualTo(testMethod);
                        foundMethod.set(true);
                        // It doesn't matter if a response is actually returned.
                        return null;
                    }
                });

        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertThat(foundMethod.get()).isTrue();
    }

    @Test
    @SmallTest
    public void testAddHeader() {
        String headerKey = "HEADERNAME";
        String headerValue = "HEADERVALUE";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                                .addHeader(headerKey, headerValue)
                                .build();

        // Use an atomic because it is set in an inner class. We do not actually need atomic for a
        // multi-threaded operation here.
        AtomicBoolean foundEntry = new AtomicBoolean();
        mFakeCronetController.addResponseMatcher(
                new ResponseMatcher() {
                    @Override
                    public FakeUrlResponse getMatchingResponse(
                            String url,
                            String httpMethod,
                            List<Map.Entry<String, String>> headers,
                            byte[] body) {
                        assertThat(headers)
                                .containsExactly(Maps.immutableEntry(headerKey, headerValue));
                        foundEntry.set(true);
                        // It doesn't matter if a response is actually returned.
                        return null;
                    }
                });
        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertThat(foundEntry.get()).isTrue();
    }

    @Test
    @SmallTest
    public void testRequestDoesNotStartWhenEngineShutDown() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                                .build();

        mFakeCronetEngine.shutdown();
        IllegalStateException e = assertThrows(IllegalStateException.class, request::start);
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("This request's CronetEngine is already shutdown.");
    }

    @Test
    @SmallTest
    public void testRequestStopsWhenCanceled() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                                .build();
        callback.setAutoAdvance(false);
        request.start();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        request.cancel();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_CANCELED);
        assertThat(callback.mResponseAsString).isEmpty();
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
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasReceivedByteCountThat()
                .isEqualTo(responseText.length());
    }

    @Test
    @SmallTest
    public void testRedirectResponse() {
        // Setup the basic response.
        String responseText = "response text";
        String redirectLocation = "/redirect_location";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder()
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertThat(callback.mResponseAsString).isEqualTo(redirectText);
    }

    @Test
    @SmallTest
    public void testRedirectResponseWithNoHeaderFails() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder()
                        .setResponseBody(responseText.getBytes())
                        .setHttpStatusCode(300)
                        .build();

        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertThat(callback.mResponseStep).isEqualTo(TestUrlRequestCallback.ResponseStep.ON_FAILED);
    }

    @Test
    @SmallTest
    public void testResponseLongerThanBuffer() {
        // Build a long response string that is 3x the buffer size.
        final int bufferStringLengthMultiplier = 3;
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String longResponseString =
                new String(new char[callback.mReadBufferSize * bufferStringLengthMultiplier]);

        String longResponseUrl = "https://www.longResponseUrl.com";

        FakeUrlResponse reallyLongResponse =
                new FakeUrlResponse.Builder()
                        .setResponseBody(longResponseString.getBytes())
                        .build();
        mFakeCronetController.addResponseForUrl(reallyLongResponse, longResponseUrl);

        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(
                                        longResponseUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        // Asserts that read happens bufferStringLengthMultiplier times
        assertReadCalled(bufferStringLengthMultiplier, callback, request);

        callback.startNextRead(request);
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(Objects.equals(callback.mResponseAsString, longResponseString)).isTrue();
    }

    @Test
    @SmallTest
    public void testStatusInvalidBeforeStart() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                                .build();
        request.start();
        checkStatus(request, Status.IDLE);
        callback.setAutoAdvance(true);
        callback.startNextRead(request);
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusIdleWhenWaitingForRedirect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String initialURL = "initialURL";
        String secondURL = "secondURL";
        mFakeCronetController.addRedirectResponse(secondURL, initialURL);
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(initialURL, callback, callback.getExecutor())
                                .build();

        request.start();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RECEIVED_REDIRECT);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();
        checkStatus(request, Status.INVALID);
    }

    @Test
    @SmallTest
    public void testIsDoneWhenComplete() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder("", callback, callback.getExecutor())
                                .build();

        request.start();
        callback.blockForDone();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(request.isDone()).isTrue();
    }

    @Test
    @SmallTest
    public void testUrlChainIsCorrectForSuccessRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        List<String> expectedUrlChain = new ArrayList<>();
        expectedUrlChain.add(testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasUrlChainThat()
                .isEqualTo(expectedUrlChain);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl1, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasUrlChainThat()
                .isEqualTo(expectedUrlChain);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(expectedResponseCode);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasHttpStatusTextThat()
                .isEqualTo(expectedResponseText);
    }

    @Test
    @SmallTest
    public void testResponseWasCachedCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setWasCached(true).build(), testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).wasCached();
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo(expectedNegotiatedProtocol);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .build();
        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks())
                .hasProxyServerThat()
                .isEqualTo(expectedProxyServer);
    }

    @Test
    @SmallTest
    public void testDirectExecutorDisabledByDefault() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAllowDirectExecutor(true);
        Executor myExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        command.run();
                    }
                };
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine.newUrlRequestBuilder("url", callback, myExecutor).build();

        request.start();
        callback.blockForDone();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        // Checks that the exception from {@link DirectPreventingExecutor} was successfully returned
        // to the callback in the onFailed method.
        assertThat(callback.mError)
                .hasCauseThat()
                .isInstanceOf(InlineExecutionProhibitedException.class);
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflow() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                                .allowDirectExecutor()
                                .build();
        request.start();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        // Asserts that read happens responseLength times
        assertReadCalled(responseLength, callback, request);

        callback.startNextRead(request);
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(callback.mResponseAsString).isEqualTo(longResponseString);
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflowWithDirectExecutor() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        callback.setAllowDirectExecutor(true);
        // Make the buffer size small so there are lots of calls to read().
        callback.mReadBufferSize = 1;
        String testUrl = "TEST_URL";
        int responseLength = 1024;
        byte[] byteArray = new byte[responseLength];
        Arrays.fill(byteArray, (byte) 1);
        String longResponseString = new String(byteArray);
        Executor myExecutor =
                new Executor() {
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
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(testUrl, callback, myExecutor)
                                .allowDirectExecutor()
                                .build();
        request.start();
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        // Asserts that read happens buffer length x multiplier times
        assertReadCalled(responseLength, callback, request);

        callback.startNextRead(request);
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
        assertThat(callback.mResponseAsString).isEqualTo(longResponseString);
    }

    @Test
    @SmallTest
    public void testDoubleReadFails() throws Exception {
        UrlRequest.Callback callback = new StubCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(
                                        "url", callback, Executors.newSingleThreadExecutor())
                                .build();
        ByteBuffer buffer = ByteBuffer.allocateDirect(32 * 1024);

        request.start();
        request.read(buffer);

        IllegalStateException e =
                assertThrows(IllegalStateException.class, () -> request.read(buffer));
        assertThat(e).hasMessageThat().isEqualTo("Invalid state transition - expected 4 but was 7");
    }

    @Test
    @SmallTest
    public void testReadWhileRedirectingFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        mFakeCronetController.addRedirectResponse("location", url);
        request.start();
        IllegalStateException e =
                assertThrows(IllegalStateException.class, () -> callback.startNextRead(request));
        assertThat(e).hasMessageThat().isEqualTo("Invalid state transition - expected 4 but was 3");
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RECEIVED_REDIRECT);
        callback.setAutoAdvance(true);
        request.followRedirect();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testShuttingDownCronetEngineWithActiveRequestFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();
        request.start();
        IllegalStateException e =
                assertThrows(IllegalStateException.class, mFakeCronetEngine::shutdown);
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
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
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();

        request.start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(404);
    }

    @Test
    @SmallTest
    public void testUploadSetDataProviderChecksForNullUploadDataProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        NullPointerException e =
                assertThrows(
                        NullPointerException.class,
                        () -> builder.setUploadDataProvider(null, callback.getExecutor()));
        assertThat(e).hasMessageThat().isEqualTo("Invalid UploadDataProvider.");
    }

    @Test
    @SmallTest
    public void testUploadSetDataProviderChecksForContentTypeHeader() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.build().start());
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("Requests with upload data must have a Content-Type.");
    }

    @Test
    @SmallTest
    public void testUploadWithEmptyBody() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEmpty();
        dataProvider.assertClosed();
    }

    @Test
    @SmallTest
    public void testUploadSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        String body = "test";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());

        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        dataProvider.addRead(body.getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(4);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadSyncReadWrongState() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        String body = "test";
        callback.setAutoAdvance(false);
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class,
                        () -> {
                            synchronized (request.mLock) {
                                request.mFakeDataSink.onReadSucceeded(false);
                            }
                        });

        assertThat(e)
                .hasMessageThat()
                .isEqualTo("onReadSucceeded() called when not awaiting a read result; in state: 2");
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
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class,
                        () -> {
                            synchronized (request.mLock) {
                                request.mFakeDataSink.onRewindSucceeded();
                            }
                        });
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("onRewindSucceeded() called when not awaiting a rewind; in state: 2");
        request.cancel();
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(dataProvider.getUploadedLength()).isEqualTo(16);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(4);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Yet another test");
    }

    @Test
    @SmallTest
    public void testUploadMultiplePiecesAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Yet another test");
    }

    @Test
    @SmallTest
    public void testUploadChangesDefaultMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(
                new ResponseMatcher() {
                    @Override
                    public FakeUrlResponse getMatchingResponse(
                            String url,
                            String httpMethod,
                            List<Map.Entry<String, String>> headers,
                            byte[] body) {
                        return new FakeUrlResponse.Builder()
                                .setResponseBody(httpMethod.getBytes())
                                .build();
                    }
                });
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("POST");
    }

    @Test
    @SmallTest
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(
                new ResponseMatcher() {
                    @Override
                    public FakeUrlResponse getMatchingResponse(
                            String url,
                            String httpMethod,
                            List<Map.Entry<String, String>> headers,
                            byte[] body) {
                        return new FakeUrlResponse.Builder()
                                .setResponseBody(httpMethod.getBytes())
                                .build();
                    }
                });
        final String method = "PUT";
        builder.setHttpMethod(method);

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("PUT");
    }

    @Test
    @SmallTest
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadRedirectAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        // 1 read call before the rewind, 1 after.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(1);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadWithBadLength() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Read upload data length 2 exceeds expected length 1");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadWithBadLengthBufferAligned() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Read upload data length 8192 exceeds expected length 8191");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadLengthFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains("Sync length failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadReadFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains("Sync read failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadReadFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains("Async read failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadReadFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains("Thrown read failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    /** This test uses a direct executor for upload, and non direct for callbacks */
    @Test
    @SmallTest
    public void testDirectExecutorUploadProhibitedByDefault() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        Executor myExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        command.run();
                    }
                };
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Inline execution is prohibited for this request");
        assertThat(callback.getResponseInfo()).isNull();
    }

    /** This test uses a direct executor for callbacks, and non direct for upload */
    @Test
    @SmallTest
    public void testDirectExecutorProhibitedByDefault() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Everything submitted to this executor will be executed immediately on the thread
        // that submitted the Runnable (blocking it until the runnable completes)
        Executor directExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        command.run();
                    }
                };
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(url, callback, directExecutor);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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
        assertThat(callback.mError).hasMessageThat().contains("Exception posting task to executor");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Inline execution is prohibited for this request");
        assertThat(callback.getResponseInfo()).isNull();
        dataProvider.assertClosed();
    }

    @Test
    @SmallTest
    public void testDirectExecutorAllowed() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAllowDirectExecutor(true);
        Executor myExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        command.run();
                    }
                };
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
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

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    @Test
    @SmallTest
    public void testUploadRewindFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError).hasCauseThat().hasMessageThat().contains("Sync rewind failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadRewindFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Async rewind failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadRewindFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String redirectUrl = "redirectUrl";
        String echoBodyUrl = "echobody";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                redirectUrl, callback, callback.getExecutor());
        mFakeCronetController.addRedirectResponse(echoBodyUrl, redirectUrl);
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher(echoBodyUrl));

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Thrown rewind failure");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testUploadChunked() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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
        String url = "url";
        FakeUrlRequest.Builder builder =
                (FakeUrlRequest.Builder)
                        mFakeCronetEngine.newUrlRequestBuilder(
                                url, callback, callback.getExecutor());
        mFakeCronetController.addResponseMatcher(new EchoBodyResponseMatcher());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
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

    @Test
    @SmallTest
    public void testCancelBeforeStart_doesNotCrash() {
        // Setup the basic response.
        String responseText = "response text";
        String url = "TEST_URL";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody(responseText.getBytes()).build();
        mFakeCronetController.addResponseForUrl(response, url);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest)
                        mFakeCronetEngine
                                .newUrlRequestBuilder(url, callback, callback.getExecutor())
                                .build();

        request.cancel();
        request.start();
        callback.blockForDone();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);
    }

    /** A Cronet callback that does nothing. */
    static class StubCallback extends UrlRequest.Callback {
        @Override
        public void onRedirectReceived(
                org.chromium.net.UrlRequest urlRequest,
                UrlResponseInfo urlResponseInfo,
                String s) {}

        @Override
        public void onResponseStarted(
                org.chromium.net.UrlRequest urlRequest, UrlResponseInfo urlResponseInfo) {}

        @Override
        public void onReadCompleted(
                org.chromium.net.UrlRequest urlRequest,
                UrlResponseInfo urlResponseInfo,
                ByteBuffer byteBuffer) {}

        @Override
        public void onSucceeded(
                org.chromium.net.UrlRequest urlRequest, UrlResponseInfo urlResponseInfo) {}

        @Override
        public void onFailed(
                org.chromium.net.UrlRequest urlRequest,
                UrlResponseInfo urlResponseInfo,
                CronetException e) {}
    }
}
