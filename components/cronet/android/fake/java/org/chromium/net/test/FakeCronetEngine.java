// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;

import androidx.annotation.GuardedBy;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.NetworkQualityRttListener;
import org.chromium.net.NetworkQualityThroughputListener;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.CronetEngineBuilderImpl;
import org.chromium.net.impl.ImplVersion;
import org.chromium.net.impl.UrlRequestBase;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * Fake {@link CronetEngine}. This implements CronetEngine.
 */
final class FakeCronetEngine extends CronetEngineBase {
    /**
     * Builds a {@link FakeCronetEngine}. This implements CronetEngine.Builder.
     */
    static class Builder extends CronetEngineBuilderImpl {
        private FakeCronetController mController;

        /**
         * Builder for {@link FakeCronetEngine}.
         *
         * @param context Android {@link Context}.
         */
        Builder(Context context) {
            super(context);
        }

        @Override
        public FakeCronetEngine build() {
            return new FakeCronetEngine(this);
        }

        void setController(FakeCronetController controller) {
            mController = controller;
        }
    }

    private final FakeCronetController mController;
    private final ExecutorService mExecutorService;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private boolean mIsShutdown;

    @GuardedBy("mLock")
    private int mActiveRequestCount;

    /**
     * Creates a {@link FakeCronetEngine}. Used when {@link FakeCronetEngine} is created with the
     * {@link FakeCronetEngine.Builder}.
     *
     * @param builder a {@link CronetEngineBuilderImpl} to build this {@link CronetEngine}
     *                implementation from.
     */
    private FakeCronetEngine(FakeCronetEngine.Builder builder) {
        if (builder.mController != null) {
            mController = builder.mController;
        } else {
            mController = new FakeCronetController();
        }
        mExecutorService = new ThreadPoolExecutor(
                /* corePoolSize= */ 1,
                /* maximumPoolSize= */ 5,
                /* keepAliveTime= */ 50, TimeUnit.SECONDS, new LinkedBlockingQueue<Runnable>(),
                new ThreadFactory() {
                    @Override
                    public Thread newThread(final Runnable r) {
                        return Executors.defaultThreadFactory().newThread(new Runnable() {
                            @Override
                            public void run() {
                                Thread.currentThread().setName("FakeCronetEngine");
                                r.run();
                            }
                        });
                    }
                });
        FakeCronetController.addFakeCronetEngine(this);
    }

    /**
     * Gets the controller associated with this instance that will be used for responses to
     * {@link UrlRequest}s.
     *
     * @return the {@link FakeCronetCntroller} that controls this {@link FakeCronetEngine}.
     */
    FakeCronetController getController() {
        return mController;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor) {
        synchronized (mLock) {
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine has been shutdown and can no longer be "
                        + "used.");
            }
            throw new UnsupportedOperationException(
                    "The bidirectional stream API is not supported by the Fake implementation "
                    + "of CronetEngine.");
        }
    }

    @Override
    public String getVersionString() {
        return "FakeCronet/" + ImplVersion.getCronetVersionWithLastChange();
    }

    @Override
    public void shutdown() {
        synchronized (mLock) {
            if (mActiveRequestCount != 0) {
                throw new IllegalStateException("Cannot shutdown with active requests.");
            } else {
                mIsShutdown = true;
            }
        }
        mExecutorService.shutdown();
        FakeCronetController.removeFakeCronetEngine(this);
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {}

    @Override
    public void startNetLogToDisk(String dirPath, boolean logAll, int maxSize) {}

    @Override
    public void stopNetLog() {}

    @Override
    public byte[] getGlobalMetricsDeltas() {
        return new byte[0];
    }

    @Override
    public int getEffectiveConnectionType() {
        return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    }

    @Override
    public int getHttpRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public int getTransportRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public int getDownstreamThroughputKbps() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    @Override
    public void configureNetworkQualityEstimatorForTesting(boolean useLocalHostRequests,
            boolean useSmallerResponses, boolean disableOfflineCheck) {}

    @Override
    public void addRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void addThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    // TODO(crbug.com/669707) Instantiate a fake CronetHttpUrlConnection wrapping a FakeUrlRequest
    // here.
    @Override
    public URLConnection openConnection(URL url) throws IOException {
        throw new UnsupportedOperationException(
                "The openConnection API is not supported by the Fake implementation of "
                + "CronetEngine.");
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        throw new UnsupportedOperationException(
                "The openConnection API is not supported by the Fake implementation of "
                + "CronetEngine.");
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        throw new UnsupportedOperationException(
                "The URLStreamHandlerFactory API is not supported by the Fake implementation of "
                + "CronetEngine.");
    }

    @Override
    protected UrlRequestBase createRequest(String url, UrlRequest.Callback callback,
            Executor userExecutor, int priority, Collection<Object> connectionAnnotations,
            boolean disableCache, boolean disableConnectionMigration, boolean allowDirectExecutor,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, RequestFinishedInfo.Listener requestFinishedListener) {
        synchronized (mLock) {
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine has been shutdown and can no longer be "
                        + "used.");
            }
            return new FakeUrlRequest(callback, userExecutor, mExecutorService, url,
                    allowDirectExecutor, trafficStatsTagSet, trafficStatsTag, trafficStatsUidSet,
                    trafficStatsUid, mController, this);
        }
    }

    @Override
    protected ExperimentalBidirectionalStream createBidirectionalStream(String url,
            BidirectionalStream.Callback callback, Executor executor, String httpMethod,
            List<Map.Entry<String, String>> requestHeaders, @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush, Collection<Object> connectionAnnotations,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid) {
        synchronized (mLock) {
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine has been shutdown and can no longer be "
                        + "used.");
            }
            throw new UnsupportedOperationException(
                    "The BidirectionalStream API is not supported by the Fake implementation of "
                    + "CronetEngine.");
        }
    }

    /**
     * Mark request as started to prevent shutdown when there are active
     * requests, only if the engine is not shutdown.
     *
     * @return true if the engine is not shutdown and the request is marked as started.
     */
    boolean startRequest() {
        synchronized (mLock) {
            if (!mIsShutdown) {
                mActiveRequestCount++;
                return true;
            }
            return false;
        }
    }

    /**
     * Mark request as finished to allow shutdown when there are no active
     * requests.
     */
    void onRequestDestroyed() {
        synchronized (mLock) {
            // Sanity check. We should not be able to shutdown if there are still running requests.
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine was shutdown. All requests must have been "
                        + "complete.");
            }
            mActiveRequestCount--;
        }
    }
}
