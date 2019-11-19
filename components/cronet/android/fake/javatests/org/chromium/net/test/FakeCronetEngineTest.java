// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetException;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.ImplVersion;

import java.net.Proxy;
import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Test functionality of {@link FakeCronetEngine}.
 */
@RunWith(AndroidJUnit4.class)
public class FakeCronetEngineTest {
    Context mContext;
    FakeCronetEngine mFakeCronetEngine;
    UrlRequest.Callback mCallback;
    ExecutorService mExecutor;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mFakeCronetEngine =
                (FakeCronetEngine) new FakeCronetProvider(mContext).createBuilder().build();
        mCallback = new UrlRequest.Callback() {
            @Override
            public void onRedirectReceived(
                    UrlRequest request, UrlResponseInfo info, String newLocationUrl) {}

            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {}

            @Override
            public void onReadCompleted(
                    UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {}

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {}

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {}

            @Override
            public void onCanceled(UrlRequest request, UrlResponseInfo info) {}
        };
        mExecutor = Executors.newSingleThreadExecutor();
    }

    @Test
    @SmallTest
    public void testShutdownEngineThrowsExceptionWhenApiCalled() {
        mFakeCronetEngine.shutdown();

        try {
            mFakeCronetEngine.newUrlRequestBuilder("", mCallback, mExecutor).build();
            fail("newUrlRequestBuilder API not checked for shutdown engine.");
        } catch (IllegalStateException e) {
            assertEquals(
                    "This instance of CronetEngine has been shutdown and can no longer be used.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testShutdownEngineThrowsExceptionWhenBidirectionalStreamApiCalled() {
        mFakeCronetEngine.shutdown();

        try {
            mFakeCronetEngine.newBidirectionalStreamBuilder("", null, null);
            fail("newBidirectionalStreamBuilder API not checked for shutdown engine.");
        } catch (IllegalStateException e) {
            assertEquals(
                    "This instance of CronetEngine has been shutdown and can no longer be used.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testExceptionForNewBidirectionalStreamApi() {
        try {
            mFakeCronetEngine.newBidirectionalStreamBuilder("", null, null);
            fail("newBidirectionalStreamBuilder API should not be available.");
        } catch (UnsupportedOperationException e) {
            assertEquals("The bidirectional stream API is not supported by the Fake implementation "
                            + "of CronetEngine.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testExceptionForOpenConnectionApi() {
        try {
            mFakeCronetEngine.openConnection(null);
            fail("openConnection API should not be available.");
        } catch (Exception e) {
            assertEquals("The openConnection API is not supported by the Fake implementation of "
                            + "CronetEngine.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testExceptionForOpenConnectionApiWithProxy() {
        try {
            mFakeCronetEngine.openConnection(null, Proxy.NO_PROXY);
            fail("openConnection API  should not be available.");
        } catch (Exception e) {
            assertEquals("The openConnection API is not supported by the Fake implementation of "
                            + "CronetEngine.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testExceptionForCreateStreamHandlerFactoryApi() {
        try {
            mFakeCronetEngine.createURLStreamHandlerFactory();
            fail("createURLStreamHandlerFactory API  should not be available.");
        } catch (UnsupportedOperationException e) {
            assertEquals(
                    "The URLStreamHandlerFactory API is not supported by the Fake implementation of"
                            + " CronetEngine.",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testGetVersionString() {
        assertEquals("FakeCronet/" + ImplVersion.getCronetVersionWithLastChange(),
                mFakeCronetEngine.getVersionString());
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
        assertTrue(mFakeCronetEngine.getGlobalMetricsDeltas().length == 0);
    }

    @Test
    @SmallTest
    public void testGetEffectiveConnectionType() {
        assertEquals(FakeCronetEngine.EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                mFakeCronetEngine.getEffectiveConnectionType());
    }

    @Test
    @SmallTest
    public void testGetHttpRttMs() {
        assertEquals(FakeCronetEngine.CONNECTION_METRIC_UNKNOWN, mFakeCronetEngine.getHttpRttMs());
    }

    @Test
    @SmallTest
    public void testGetTransportRttMs() {
        assertEquals(
                FakeCronetEngine.CONNECTION_METRIC_UNKNOWN, mFakeCronetEngine.getTransportRttMs());
    }

    @Test
    @SmallTest
    public void testGetDownstreamThroughputKbps() {
        assertEquals(FakeCronetEngine.CONNECTION_METRIC_UNKNOWN,
                mFakeCronetEngine.getDownstreamThroughputKbps());
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
    public void testAddRequestFinishedListener() {
        mFakeCronetEngine.addRequestFinishedListener(null);
    }

    @Test
    @SmallTest
    public void testRemoveRequestFinishedListener() {
        mFakeCronetEngine.removeRequestFinishedListener(null);
    }

    @Test
    @SmallTest
    public void testShutdownBlockedWhenRequestCountNotZero() {
        // Start a request and verify the engine can't be shutdown.
        assertTrue(mFakeCronetEngine.startRequest());
        try {
            mFakeCronetEngine.shutdown();
            fail("Shutdown not checked for active requests.");
        } catch (IllegalStateException e) {
            assertEquals("Cannot shutdown with active requests.", e.getMessage());
        }

        // Finish the request and verify the engine can be shutdown.
        mFakeCronetEngine.onRequestDestroyed();
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testCantStartRequestAfterEngineShutdown() {
        mFakeCronetEngine.shutdown();
        assertFalse(mFakeCronetEngine.startRequest());
    }

    @Test
    @SmallTest
    public void testCantDecrementOnceShutdown() {
        mFakeCronetEngine.shutdown();

        try {
            mFakeCronetEngine.onRequestDestroyed();
            fail("onRequestDestroyed not checked for shutdown engine");
        } catch (IllegalStateException e) {
            assertEquals("This instance of CronetEngine was shutdown. All requests must have been "
                            + "complete.",
                    e.getMessage());
        }
    }
}
