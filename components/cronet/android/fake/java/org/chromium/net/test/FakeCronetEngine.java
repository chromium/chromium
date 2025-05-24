// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.ExperimentalUrlRequest;
import org.chromium.net.NetworkQualityRttListener;
import org.chromium.net.NetworkQualityThroughputListener;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.CronetEngineBuilderImpl;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.ImplVersion;
import org.chromium.net.impl.RefCountDelegate;
import org.chromium.net.impl.VersionSafeCallbacks;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/** Fake {@link CronetEngine}. This implements CronetEngine. */
final class FakeCronetEngine extends CronetEngineBase {
    /** Builds a {@link FakeCronetEngine}. This implements CronetEngine.Builder. */
    static class Builder extends CronetEngineBuilderImpl {
        private FakeCronetController mController;

        /**
         * Builder for {@link FakeCronetEngine}.
         *
         * @param context Android {@link Context}.
         */
        Builder(Context context) {
            super(context, CronetSource.CRONET_SOURCE_FAKE);
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

    /**
     * The number of started requests where the terminal callback (i.e.
     * onSucceeded/onCancelled/onFailed) has not yet been called.
     */
    @GuardedBy("mLock")
    private int mRunningRequestCount;

    /*
     * The number of started requests where the terminal callbacks (i.e.
     * onSucceeded/onCancelled/onFailed, request finished listeners) have not
     * all returned yet.
     *
     * By definition this is always greater than or equal to
     * mRunningRequestCount. The difference between the two is the number of
     * terminal callbacks that are currently running.
     */
    @GuardedBy("mLock")
    private int mActiveRequestCount;

    @GuardedBy("mLock")
    private final Map<
                    RequestFinishedInfo.Listener, VersionSafeCallbacks.RequestFinishedInfoListener>
            mFinishedListenerMap = new HashMap<>();

    /**
     * Creates a {@link FakeCronetEngine}. Used when {@link FakeCronetEngine} is created with the
     * {@link FakeCronetEngine.Builder}.
     *
     * @param builder a {@link CronetEngineBuilderImpl} to build this {@link CronetEngine}
     *     implementation from.
     */
    @SuppressWarnings("ErroneousThreadPoolConstructorChecker")
    private FakeCronetEngine(FakeCronetEngine.Builder builder) {
        if (builder.mController != null) {
            mController = builder.mController;
        } else {
            mController = new FakeCronetController();
        }
        // TODO(ErroneousThreadPoolConstructorChecker): Thread pool size will never go beyond
        // corePoolSize if an unbounded queue is used
        mExecutorService =
                new ThreadPoolExecutor(
                        /* corePoolSize= */ 1,
                        /* maximumPoolSize= */ 5,
                        /* keepAliveTime= */ 50,
                        TimeUnit.SECONDS,
                        new LinkedBlockingQueue<Runnable>(),
                        new ThreadFactory() {
                            @Override
                            public Thread newThread(final Runnable r) {
                                return Executors.defaultThreadFactory()
                                        .newThread(
                                                () -> {
                                                    Thread.currentThread()
                                                            .setName("FakeCronetEngine");
                                                    r.run();
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
            if (mRunningRequestCount != 0) {
                throw new IllegalStateException("Cannot shutdown with running requests.");
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
    public void bindToNetwork(long networkHandle) {
        throw new UnsupportedOperationException(
                "The multi-network API is not supported by the Fake implementation "
                        + "of Cronet Engine");
    }

    @Override
    public void configureNetworkQualityEstimatorForTesting(
            boolean useLocalHostRequests,
            boolean useSmallerResponses,
            boolean disableOfflineCheck) {}

    @Override
    public void addRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {}

    @Override
    public void addThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {}

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("Listener must not be null");
        }
        synchronized (mLock) {
            mFinishedListenerMap.put(
                    listener, new VersionSafeCallbacks.RequestFinishedInfoListener(listener));
        }
    }

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        if (listener == null) {
            throw new IllegalArgumentException("Listener must not be null");
        }
        synchronized (mLock) {
            mFinishedListenerMap.remove(listener);
        }
    }

    boolean hasRequestFinishedListeners() {
        synchronized (mLock) {
            return !mFinishedListenerMap.isEmpty();
        }
    }

    void reportRequestFinished(
            RequestFinishedInfo requestInfo, RefCountDelegate inflightDoneCallbackCount) {
        synchronized (mLock) {
            for (RequestFinishedInfo.Listener listener : mFinishedListenerMap.values()) {
                inflightDoneCallbackCount.increment();
                listener.getExecutor()
                        .execute(
                                () -> {
                                    try {
                                        listener.onRequestFinished(requestInfo);
                                    } finally {
                                        inflightDoneCallbackCount.decrement();
                                    }
                                });
            }
        }
    }

    // TODO(crbug.com/41288733) Instantiate a fake CronetHttpUrlConnection wrapping a FakeUrlRequest
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
    protected ExperimentalUrlRequest createRequest(
            String url,
            UrlRequest.Callback callback,
            Executor userExecutor,
            int priority,
            Collection<Object> connectionAnnotations,
            boolean disableCache,
            boolean disableConnectionMigration,
            boolean allowDirectExecutor,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            boolean trafficStatsUidSet,
            int trafficStatsUid,
            RequestFinishedInfo.Listener requestFinishedListener,
            int idempotency,
            long networkHandle,
            String method,
            ArrayList<Map.Entry<String, String>> requestHeaders,
            UploadDataProvider uploadDataProvider,
            Executor uploadDataProviderExecutor,
            byte[] dictionarySha256Hash,
            ByteBuffer sharedDictionary,
            @NonNull String sharedDictionaryId) {
        if (networkHandle != DEFAULT_NETWORK_HANDLE) {
            throw new UnsupportedOperationException(
                    "The multi-network API is not supported by the Fake implementation "
                            + "of Cronet Engine");
        }

        synchronized (mLock) {
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine has been shutdown and can no longer be "
                                + "used.");
            }
            return new FakeUrlRequest(
                    callback,
                    userExecutor,
                    mExecutorService,
                    url,
                    allowDirectExecutor,
                    trafficStatsTagSet,
                    trafficStatsTag,
                    trafficStatsUidSet,
                    trafficStatsUid,
                    mController,
                    this,
                    connectionAnnotations,
                    method,
                    requestHeaders,
                    uploadDataProvider,
                    uploadDataProviderExecutor);
        }
    }

    @Override
    public int getActiveRequestCount() {
        synchronized (mLock) {
            return mActiveRequestCount;
        }
    }

    @Override
    protected ExperimentalBidirectionalStream createBidirectionalStream(
            String url,
            BidirectionalStream.Callback callback,
            Executor executor,
            String httpMethod,
            List<Map.Entry<String, String>> requestHeaders,
            @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush,
            Collection<Object> connectionAnnotations,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            boolean trafficStatsUidSet,
            int trafficStatsUid,
            long networkHandle) {
        if (networkHandle != DEFAULT_NETWORK_HANDLE) {
            throw new UnsupportedOperationException(
                    "The multi-network API is not supported by the Fake implementation "
                            + "of Cronet Engine");
        }
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
                mRunningRequestCount++;
                return true;
            }
            return false;
        }
    }

    /**
     * Mark request as destroyed to allow shutdown when there are no running
     * requests. Should be called *before* the terminal callback is called, so
     * that users can call shutdown() from the terminal callback.
     */
    void onRequestDestroyed() {
        synchronized (mLock) {
            // Verification check. We should not be able to shutdown if there are still running
            // requests.
            if (mIsShutdown) {
                throw new IllegalStateException(
                        "This instance of CronetEngine was shutdown. All requests must have been "
                                + "complete.");
            }
            mRunningRequestCount--;
        }
    }

    /**
     * Mark request as finished for the purposes of getActiveRequestCount().
     * Should be called *after* the terminal callback returns.
     */
    void onRequestFinished() {
        synchronized (mLock) {
            mActiveRequestCount--;
        }
    }
}
