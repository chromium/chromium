// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.TestRequestFinishedListener;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.ImplVersion;

import java.net.Proxy;
import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Test functionality of {@link FakeCronetEngine}. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class FakeCronetEngineTest {
    Context mContext;
    FakeCronetEngine mFakeCronetEngine;
    UrlRequest.Callback mCallback;
    ExecutorService mExecutor;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFakeCronetEngine =
                (FakeCronetEngine) new FakeCronetProvider(mContext).createBuilder().build();
        mCallback =
                new UrlRequest.Callback() {
                    @Override
                    public void onRedirectReceived(
                            UrlRequest request, UrlResponseInfo info, String newLocationUrl) {}

                    @Override
                    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                        request.read(ByteBuffer.allocate(0));
                    }

                    @Override
                    public void onReadCompleted(
                            UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {}

                    @Override
                    public void onSucceeded(UrlRequest request, UrlResponseInfo info) {}

                    @Override
                    public void onFailed(
                            UrlRequest request, UrlResponseInfo info, CronetException error) {}

                    @Override
                    public void onCanceled(UrlRequest request, UrlResponseInfo info) {}
                };
        mExecutor = Executors.newSingleThreadExecutor();
    }

    @Test
    @SmallTest
    public void testShutdownEngineThrowsExceptionWhenApiCalled() {
        mFakeCronetEngine.shutdown();

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class,
                        () ->
                                mFakeCronetEngine
                                        .newUrlRequestBuilder("", mCallback, mExecutor)
                                        .build());
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "This instance of CronetEngine has been shutdown and can no longer be"
                                + " used.");
    }

    @Test
    @SmallTest
    public void testShutdownEngineThrowsExceptionWhenBidirectionalStreamApiCalled() {
        mFakeCronetEngine.shutdown();

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class,
                        () -> mFakeCronetEngine.newBidirectionalStreamBuilder("", null, null));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "This instance of CronetEngine has been shutdown and can no longer be"
                                + " used.");
    }

    @Test
    @SmallTest
    public void testExceptionForNewBidirectionalStreamApi() {
        UnsupportedOperationException e =
                assertThrows(
                        UnsupportedOperationException.class,
                        () -> mFakeCronetEngine.newBidirectionalStreamBuilder("", null, null));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "The bidirectional stream API is not supported by the Fake implementation "
                                + "of CronetEngine.");
    }

    @Test
    @SmallTest
    public void testExceptionForOpenConnectionApi() {
        Exception e = assertThrows(Exception.class, () -> mFakeCronetEngine.openConnection(null));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "The openConnection API is not supported by the Fake implementation of "
                                + "CronetEngine.");
    }

    @Test
    @SmallTest
    public void testExceptionForOpenConnectionApiWithProxy() {
        Exception e =
                assertThrows(
                        Exception.class,
                        () -> mFakeCronetEngine.openConnection(null, Proxy.NO_PROXY));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "The openConnection API is not supported by the Fake implementation of "
                                + "CronetEngine.");
    }

    @Test
    @SmallTest
    public void testExceptionForCreateStreamHandlerFactoryApi() {
        UnsupportedOperationException e =
                assertThrows(
                        UnsupportedOperationException.class,
                        () -> mFakeCronetEngine.createURLStreamHandlerFactory());
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "The URLStreamHandlerFactory API is not supported by the Fake"
                                + " implementation of CronetEngine.");
    }

    @Test
    @SmallTest
    public void testGetVersionString() {
        assertThat(mFakeCronetEngine.getVersionString())
                .isEqualTo("FakeCronet/" + ImplVersion.getCronetVersionWithLastChange());
    }

    @Test
    @SmallTest
    public void testStartNetLogToFile() {
        mFakeCronetEngine.startNetLogToFile("", false);
    }

    @Test
    @SmallTest
    public void testStartNetLogToDisk() {
        mFakeCronetEngine.startNetLogToDisk("", false, 0);
    }

    @Test
    @SmallTest
    public void testStopNetLog() {
        mFakeCronetEngine.stopNetLog();
    }

    @Test
    @SmallTest
    public void testGetGlobalMetricsDeltas() {
        assertThat(mFakeCronetEngine.getGlobalMetricsDeltas()).isEmpty();
    }

    @Test
    @SmallTest
    public void testGetEffectiveConnectionType() {
        assertThat(mFakeCronetEngine.getEffectiveConnectionType())
                .isEqualTo(FakeCronetEngine.EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    }

    @Test
    @SmallTest
    public void testGetHttpRttMs() {
        assertThat(mFakeCronetEngine.getHttpRttMs())
                .isEqualTo(FakeCronetEngine.CONNECTION_METRIC_UNKNOWN);
    }

    @Test
    @SmallTest
    public void testGetTransportRttMs() {
        assertThat(mFakeCronetEngine.getTransportRttMs())
                .isEqualTo(FakeCronetEngine.CONNECTION_METRIC_UNKNOWN);
    }

    @Test
    @SmallTest
    public void testGetDownstreamThroughputKbps() {
        assertThat(mFakeCronetEngine.getDownstreamThroughputKbps())
                .isEqualTo(FakeCronetEngine.CONNECTION_METRIC_UNKNOWN);
    }

    @Test
    @SmallTest
    public void testConfigureNetworkQualityEstimatorForTesting() {
        mFakeCronetEngine.configureNetworkQualityEstimatorForTesting(false, false, false);
    }

    @Test
    @SmallTest
    public void testAddRttListener() {
        mFakeCronetEngine.addRttListener(null);
    }

    @Test
    @SmallTest
    public void testRemoveRttListener() {
        mFakeCronetEngine.removeRttListener(null);
    }

    @Test
    @SmallTest
    public void testAddThroughputListener() {
        mFakeCronetEngine.addThroughputListener(null);
    }

    @Test
    @SmallTest
    public void testRemoveThroughputListener() {
        mFakeCronetEngine.removeThroughputListener(null);
    }

    @Test
    @SmallTest
    public void testShutdownBlockedWhenRequestCountNotZero() {
        // Start a request and verify the engine can't be shutdown.
        assertThat(mFakeCronetEngine.startRequest()).isTrue();
        IllegalStateException e =
                assertThrows(IllegalStateException.class, () -> mFakeCronetEngine.shutdown());
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");

        // Finish the request and verify the engine can be shutdown.
        mFakeCronetEngine.onRequestDestroyed();
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testAddRequestFinishedListenerShouldAffectSize() {
        mFakeCronetEngine.addRequestFinishedListener(
                new RequestFinishedInfo.Listener(mExecutor) {
                    @Override
                    public void onRequestFinished(RequestFinishedInfo requestInfo) {}
                });
        assertThat(mFakeCronetEngine.hasRequestFinishedListeners()).isTrue();
    }

    @Test
    @SmallTest
    public void testRemoveRequestFinishedListenerShouldAffectSize() {
        RequestFinishedInfo.Listener listener =
                new RequestFinishedInfo.Listener(mExecutor) {
                    @Override
                    public void onRequestFinished(RequestFinishedInfo requestInfo) {}
                };
        mFakeCronetEngine.addRequestFinishedListener(listener);
        assertThat(mFakeCronetEngine.hasRequestFinishedListeners()).isTrue();
        mFakeCronetEngine.removeRequestFinishedListener(listener);
        assertThat(mFakeCronetEngine.hasRequestFinishedListeners()).isFalse();
    }

    @Test
    @SmallTest
    public void testAddNullRequestFinishedListenerShouldThrowException() {
        assertThrows(
                IllegalArgumentException.class,
                () -> mFakeCronetEngine.addRequestFinishedListener(null));
    }

    @Test
    @SmallTest
    public void testRemoveNullRequestFinishedListenerShouldThrowException() {
        assertThrows(
                IllegalArgumentException.class,
                () -> mFakeCronetEngine.removeRequestFinishedListener(null));
    }

    @Test
    @SmallTest
    public void testFinishedRequestListener() {
        String url = "broken_url";
        String annotation = "some_annotation";
        TestRequestFinishedListener listener =
                new TestRequestFinishedListener(mExecutor) {
                    @Override
                    public void onRequestFinished(RequestFinishedInfo requestInfo) {
                        super.onRequestFinished(requestInfo);
                        assertThat(requestInfo.getUrl()).isEqualTo(url);
                        assertThat(requestInfo.getAnnotations()).contains(annotation);
                    }
                };
        mFakeCronetEngine.addRequestFinishedListener(listener);
        UrlRequest urlRequest =
                mFakeCronetEngine
                        .newUrlRequestBuilder(url, mCallback, mExecutor)
                        .addRequestAnnotation(annotation)
                        .build();
        urlRequest.start();
        listener.blockUntilDone();
    }

    @Test
    @SmallTest
    public void testFinishedRequestListenerFailWithException() {
        UrlRequest.Callback exceptionCallBack =
                new FakeUrlRequestTest.StubCallback() {
                    @Override
                    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                        throw new IllegalStateException("Throwing an exception");
                    }
                };
        TestRequestFinishedListener listener =
                new TestRequestFinishedListener(mExecutor) {
                    @Override
                    public void onRequestFinished(RequestFinishedInfo requestInfo) {
                        super.onRequestFinished(requestInfo);
                        assertThat(requestInfo.getException())
                                .hasMessageThat()
                                .isEqualTo("Exception received from UrlRequest.Callback");
                    }
                };
        mFakeCronetEngine.addRequestFinishedListener(listener);
        UrlRequest urlRequest =
                mFakeCronetEngine.newUrlRequestBuilder("", exceptionCallBack, mExecutor).build();
        urlRequest.start();
        listener.blockUntilDone();
    }

    @Test
    @SmallTest
    public void testCantStartRequestAfterEngineShutdown() {
        mFakeCronetEngine.shutdown();
        assertThat(mFakeCronetEngine.startRequest()).isFalse();
    }

    @Test
    @SmallTest
    public void testCantDecrementOnceShutdown() {
        mFakeCronetEngine.shutdown();

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class, () -> mFakeCronetEngine.onRequestDestroyed());
        assertThat(e)
                .hasMessageThat()
                .isEqualTo(
                        "This instance of CronetEngine was shutdown. All requests must have been "
                                + "complete.");
    }
}
