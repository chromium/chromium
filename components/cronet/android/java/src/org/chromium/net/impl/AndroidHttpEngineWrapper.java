// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.Network;
import android.net.http.HttpEngine;
import android.os.Process;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresExtension;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetEngine;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidHttpEngineWrapper extends CronetEngineBase {
    private static final String TAG = "HttpEngineWrapper";

    private static boolean sNetlogUnsupportedLogged;
    private static boolean sGlobalMetricsUnsupportedLogged;

    private final HttpEngine mBackend;
    private final int mThreadPriority;
    // The thread that priority has been set on.
    private Thread mPriorityThread;
    private final Map<
                    RequestFinishedInfo.Listener, VersionSafeCallbacks.RequestFinishedInfoListener>
            mFinishedListenerMap = Collections.synchronizedMap(new HashMap<>());

    public AndroidHttpEngineWrapper(HttpEngine backend, int threadPriority) {
        mBackend = backend;
        mThreadPriority = threadPriority;
    }

    @Override
    public String getVersionString() {
        return HttpEngine.getVersionString();
    }

    @Override
    public void shutdown() {
        mBackend.shutdown();
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {
        // TODO: Hidden API access
        if (!sNetlogUnsupportedLogged) {
            Log.i(TAG, "Netlog is unsupported when HttpEngineNativeProvider is used.");
            sNetlogUnsupportedLogged = true;
        }
    }

    @Override
    public void stopNetLog() {
        // TODO: Hidden API access
    }

    @Override
    public byte[] getGlobalMetricsDeltas() {
        // TODO: Hidden API access
        if (!sGlobalMetricsUnsupportedLogged) {
            Log.i(
                    TAG,
                    "GlobalMetricsDelta is unsupported when HttpEngineNativeProvider is used. An"
                            + " empty protobuf is returned.");
            sGlobalMetricsUnsupportedLogged = true;
        }
        return new byte[0];
    }

    @Override
    public void bindToNetwork(long networkHandle) {
        mBackend.bindToNetwork(getNetwork(networkHandle));
    }

    @Override
    public URLConnection openConnection(URL url) throws IOException {
        return CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> mBackend.openConnection(url), IOException.class);
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        // HttpEngine doesn't expose an openConnection(URL, Proxy) method. To maintain compatibility
        // copy-paste CronetUrlRequestContext's logic here.
        if (proxy.type() != Proxy.Type.DIRECT) {
            throw new UnsupportedOperationException();
        }
        String protocol = url.getProtocol();
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return openConnection(url);
        }
        throw new UnsupportedOperationException("Unexpected protocol:" + protocol);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        return mBackend.createUrlStreamHandlerFactory();
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, org.chromium.net.BidirectionalStream.Callback callback, Executor executor) {
        return new BidirectionalStreamBuilderImpl(
                url, callback, maybeWrapWithPrioritySettingExecutor(executor), this);
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, org.chromium.net.UrlRequest.Callback callback, Executor executor) {
        return super.newUrlRequestBuilder(
                url, callback, maybeWrapWithPrioritySettingExecutor(executor));
    }

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        mFinishedListenerMap.put(
                listener, new VersionSafeCallbacks.RequestFinishedInfoListener(listener));
    }

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        mFinishedListenerMap.remove(listener);
    }

    void reportRequestFinished(
            RequestFinishedInfo requestInfo,
            VersionSafeCallbacks.RequestFinishedInfoListener extraRequestListener) {
        ArrayList<VersionSafeCallbacks.RequestFinishedInfoListener> currentListeners =
                new ArrayList<>();
        synchronized (mFinishedListenerMap) {
            currentListeners.addAll(mFinishedListenerMap.values());
        }
        if (extraRequestListener != null) {
            currentListeners.add(extraRequestListener);
        }
        for (final VersionSafeCallbacks.RequestFinishedInfoListener listener : currentListeners) {
            try {
                listener.getExecutor()
                        .execute(
                                () -> {
                                    try {
                                        listener.onRequestFinished(requestInfo);
                                    } catch (Exception e) {
                                        Log.e(TAG, "Exception thrown from observation task", e);
                                    }
                                });
            } catch (RejectedExecutionException failException) {
                Log.e(TAG, "Exception posting task to executor", failException);
            }
        }
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream createBidirectionalStream(
            String url,
            BidirectionalStream.Callback callback,
            Executor executor,
            String httpMethod,
            List<Entry<String, String>> requestHeaders,
            @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush,
            Collection<Object> requestAnnotations,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            boolean trafficStatsUidSet,
            int trafficStatsUid,
            long networkHandle /* TODO(b/309112420): add to HttpEngine */) {
        AndroidBidirectionalStreamCallbackWrapper wrappedCallback =
                new AndroidBidirectionalStreamCallbackWrapper(callback);

        android.net.http.BidirectionalStream.Builder streamBuilder =
                mBackend.newBidirectionalStreamBuilder(url, executor, wrappedCallback);
        streamBuilder.setHttpMethod(httpMethod);
        for (Map.Entry<String, String> header : requestHeaders) {
            streamBuilder.addHeader(header.getKey(), header.getValue());
        }
        streamBuilder.setPriority(priority);
        streamBuilder.setDelayRequestHeadersUntilFirstFlushEnabled(
                delayRequestHeadersUntilFirstFlush);
        if (trafficStatsTagSet) {
            streamBuilder.setTrafficStatsTag(trafficStatsTag);
        }
        if (trafficStatsUidSet) {
            streamBuilder.setTrafficStatsUid(trafficStatsUid);
        }

        return AndroidBidirectionalStreamWrapper.createAndAddToCallback(
                streamBuilder.build(), wrappedCallback, this, url, requestAnnotations);
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest createRequest(
            String url,
            UrlRequest.Callback callback,
            Executor executor,
            @RequestPriority int priority,
            Collection<Object> requestAnnotations,
            boolean disableCache,
            boolean disableConnectionMigration /* not in HttpEngine */,
            boolean allowDirectExecutor,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            boolean trafficStatsUidSet,
            int trafficStatsUid,
            @Nullable RequestFinishedInfo.Listener requestFinishedListener,
            @Idempotency int idempotency /* not in HttpEngine */,
            long networkHandle,
            String method,
            ArrayList<Map.Entry<String, String>> requestHeaders,
            UploadDataProvider uploadDataProvider,
            Executor uploadDataProviderExecutor,
            byte[] dictionarySha256Hash,
            ByteBuffer sharedDictionary,
            @NonNull String sharedDictionaryId) {
        AndroidUrlRequestCallbackWrapper wrappedCallback =
                new AndroidUrlRequestCallbackWrapper(callback);
        android.net.http.UrlRequest.Builder requestBuilder =
                mBackend.newUrlRequestBuilder(url, executor, wrappedCallback);

        requestBuilder.setPriority(priority);
        requestBuilder.setCacheDisabled(disableCache);
        requestBuilder.setDirectExecutorAllowed(allowDirectExecutor);
        if (trafficStatsTagSet) {
            requestBuilder.setTrafficStatsTag(trafficStatsTag);
        }
        if (trafficStatsUidSet) {
            requestBuilder.setTrafficStatsTag(trafficStatsUid);
        }
        requestBuilder.bindToNetwork(getNetwork(networkHandle));
        requestBuilder.setHttpMethod(method);
        for (Map.Entry<String, String> header : requestHeaders) {
            requestBuilder.addHeader(header.getKey(), header.getValue());
        }
        if (uploadDataProvider != null) {
            requestBuilder.setUploadDataProvider(
                    new AndroidUploadDataProviderWrapper(uploadDataProvider),
                    uploadDataProviderExecutor);
        }

        return AndroidUrlRequestWrapper.createAndAddToCallback(
                requestBuilder.build(),
                wrappedCallback,
                this,
                url,
                requestAnnotations,
                requestFinishedListener);
    }

    private Network getNetwork(long networkHandle) {
        // Network#fromNetworkHandle throws IAE if networkHandle does not translate to a valid
        // Network. Though, this can only happen if we're given a fake networkHandle (in which case
        // we will throw, which is fine).
        return networkHandle == CronetEngine.UNBIND_NETWORK_HANDLE
                ? null
                : Network.fromNetworkHandle(networkHandle);
    }

    /**
     * Wrap executor if user set the thread priority via {@link
     * CronetEngine.Builder#setThreadPriority(int)}. All requests/streams in an engine are either
     * wrapped or not wrapped depending on if thread priority is set.
     */
    private Executor maybeWrapWithPrioritySettingExecutor(Executor executor) {
        return mThreadPriority == Integer.MIN_VALUE
                ? executor
                : new PrioritySettingExecutor(executor);
    }

    /**
     * Set the thread priority if it has not been set before.
     *
     * @return True iff the thread priority was set.
     */
    @VisibleForTesting
    boolean setThreadPriority() {
        // Double-check that we always get called from the same thread. If this assertion fails,
        // it means we were called from a thread that is not the Cronet internal thread, which
        // is a problem because it means we could end up changing the priority of some random
        // thread we don't own.
        assert mPriorityThread == null || mPriorityThread == Thread.currentThread();
        if (mPriorityThread != null) {
            return false;
        }
        Process.setThreadPriority(mThreadPriority);
        mPriorityThread = Thread.currentThread();
        return true;
    }

    /**
     * HttpEngine does not support {@link CronetEngine.Builder#setThreadPriority). To preserve
     * compatibility with Cronet users who use this method, we reimplement the functionality using a
     * workaround where we set the priority of the first thread to call execute() which is the
     * network thread for Cronet.
     */
    private class PrioritySettingExecutor implements Executor {
        private final Executor mExecutor;

        public PrioritySettingExecutor(Executor executor) {
            mExecutor = Objects.requireNonNull(executor, "Executor is required.");
        }

        @Override
        public void execute(Runnable command) {
            setThreadPriority();
            mExecutor.execute(command);
        }
    }
}
