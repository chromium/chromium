// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.CallbackException;
import org.chromium.net.CronetException;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.NetworkException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.RequestPriority;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;

import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

import javax.annotation.concurrent.GuardedBy;

/**
 * UrlRequest using Chromium HTTP stack implementation. Could be accessed from
 * any thread on Executor. Cancel can be called from any thread.
 * All @CallByNative methods are called on native network thread
 * and post tasks with listener calls onto Executor. Upon return from listener
 * callback native request adapter is called on executive thread and posts
 * native tasks to native network thread. Because Cancel could be called from
 * any thread it is protected by mUrlRequestAdapterLock.
 */
@JNINamespace("cronet")
// Qualifies VersionSafeCallbacks.UrlRequestStatusListener which is used in onStatus, a JNI method.
@JNIAdditionalImport(VersionSafeCallbacks.class)
@VisibleForTesting
public final class CronetUrlRequest extends UrlRequestBase {
    private final boolean mAllowDirectExecutor;

    /* Native adapter object, owned by UrlRequest. */
    @GuardedBy("mUrlRequestAdapterLock")
    private long mUrlRequestAdapter;

    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mStarted;
    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mWaitingOnRedirect;
    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mWaitingOnRead;

    /*
     * Synchronize access to mUrlRequestAdapter, mStarted, mWaitingOnRedirect,
     * and mWaitingOnRead.
     */
    private final Object mUrlRequestAdapterLock = new Object();
    private final CronetUrlRequestContext mRequestContext;
    private final Executor mExecutor;

    /*
     * URL chain contains the URL currently being requested, and
     * all URLs previously requested. New URLs are added before
     * mCallback.onRedirectReceived is called.
     */
    private final List<String> mUrlChain = new ArrayList<String>();

    private final VersionSafeCallbacks.UrlRequestCallback mCallback;
    private final String mInitialUrl;
    private final int mPriority;
    private String mInitialMethod;
    private final HeadersList mRequestHeaders = new HeadersList();
    private final Collection<Object> mRequestAnnotations;
    private final boolean mDisableCache;
    private final boolean mDisableConnectionMigration;
    private final boolean mTrafficStatsTagSet;
    private final int mTrafficStatsTag;
    private final boolean mTrafficStatsUidSet;
    private final int mTrafficStatsUid;
    private final VersionSafeCallbacks.RequestFinishedInfoListener mRequestFinishedListener;

    private CronetUploadDataStream mUploadDataStream;

    private UrlResponseInfoImpl mResponseInfo;

    // These three should only be updated once with mUrlRequestAdapterLock held. They are read on
    // UrlRequest.Callback's and RequestFinishedInfo.Listener's executors after the last update.
    @RequestFinishedInfoImpl.FinishedReason
    private int mFinishedReason;
    private CronetException mException;
    private CronetMetrics mMetrics;

    /*
     * Listener callback is repeatedly invoked when each read is completed, so it
     * is cached as a member variable.
     */
    private OnReadCompletedRunnable mOnReadCompletedTask;

    @GuardedBy("mUrlRequestAdapterLock")
    private Runnable mOnDestroyedCallbackForTesting;

    private static final class HeadersList extends ArrayList<Map.Entry<String, String>> {}

    private final class OnReadCompletedRunnable implements Runnable {
        // Buffer passed back from current invocation of onReadCompleted.
        ByteBuffer mByteBuffer;

        @Override
        public void run() {
            checkCallingThread();
            // Null out mByteBuffer, to pass buffer ownership to callback or release if done.
            ByteBuffer buffer = mByteBuffer;
            mByteBuffer = null;

            try {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    mWaitingOnRead = true;
                }
                mCallback.onReadCompleted(CronetUrlRequest.this, mResponseInfo, buffer);
            } catch (Exception e) {
                onCallbackException(e);
            }
        }
    }

    CronetUrlRequest(CronetUrlRequestContext requestContext, String url, int priority,
            UrlRequest.Callback callback, Executor executor, Collection<Object> requestAnnotations,
            boolean disableCache, boolean disableConnectionMigration, boolean allowDirectExecutor,
            boolean trafficStatsTagSet, int trafficStatsTag, boolean trafficStatsUidSet,
            int trafficStatsUid, RequestFinishedInfo.Listener requestFinishedListener) {
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        if (callback == null) {
            throw new NullPointerException("Listener is required");
        }
        if (executor == null) {
            throw new NullPointerException("Executor is required");
        }

        mAllowDirectExecutor = allowDirectExecutor;
        mRequestContext = requestContext;
        mInitialUrl = url;
        mUrlChain.add(url);
        mPriority = convertRequestPriority(priority);
        mCallback = new VersionSafeCallbacks.UrlRequestCallback(callback);
        mExecutor = executor;
        mRequestAnnotations = requestAnnotations;
        mDisableCache = disableCache;
        mDisableConnectionMigration = disableConnectionMigration;
        mTrafficStatsTagSet = trafficStatsTagSet;
        mTrafficStatsTag = trafficStatsTag;
        mTrafficStatsUidSet = trafficStatsUidSet;
        mTrafficStatsUid = trafficStatsUid;
        mRequestFinishedListener = requestFinishedListener != null
                ? new VersionSafeCallbacks.RequestFinishedInfoListener(requestFinishedListener)
                : null;
    }

    @Override
    public void setHttpMethod(String method) {
        checkNotStarted();
        if (method == null) {
            throw new NullPointerException("Method is required.");
        }
        mInitialMethod = method;
    }

    @Override
    public void addHeader(String header, String value) {
        checkNotStarted();
        if (header == null) {
            throw new NullPointerException("Invalid header name.");
        }
        if (value == null) {
            throw new NullPointerException("Invalid header value.");
        }
        mRequestHeaders.add(new AbstractMap.SimpleImmutableEntry<String, String>(header, value));
    }

    @Override
    public void setUploadDataProvider(UploadDataProvider uploadDataProvider, Executor executor) {
        if (uploadDataProvider == null) {
            throw new NullPointerException("Invalid UploadDataProvider.");
        }
        if (mInitialMethod == null) {
            mInitialMethod = "POST";
        }
        mUploadDataStream = new CronetUploadDataStream(uploadDataProvider, executor, this);
    }

    @Override
    public void start() {
        synchronized (mUrlRequestAdapterLock) {
            checkNotStarted();

            try {
                mUrlRequestAdapter = CronetUrlRequestJni.get().createRequestAdapter(
                        CronetUrlRequest.this, mRequestContext.getUrlRequestContextAdapter(),
                        mInitialUrl, mPriority, mDisableCache, mDisableConnectionMigration,
                        mRequestContext.hasRequestFinishedListener()
                                || mRequestFinishedListener != null,
                        mTrafficStatsTagSet, mTrafficStatsTag, mTrafficStatsUidSet,
                        mTrafficStatsUid);
                mRequestContext.onRequestStarted();
                if (mInitialMethod != null) {
                    if (!CronetUrlRequestJni.get().setHttpMethod(
                                mUrlRequestAdapter, CronetUrlRequest.this, mInitialMethod)) {
                        throw new IllegalArgumentException("Invalid http method " + mInitialMethod);
                    }
                }

                boolean hasContentType = false;
                for (Map.Entry<String, String> header : mRequestHeaders) {
                    if (header.getKey().equalsIgnoreCase("Content-Type")
                            && !header.getValue().isEmpty()) {
                        hasContentType = true;
                    }
                    if (!CronetUrlRequestJni.get().addRequestHeader(mUrlRequestAdapter,
                                CronetUrlRequest.this, header.getKey(), header.getValue())) {
                        throw new IllegalArgumentException(
                                "Invalid header " + header.getKey() + "=" + header.getValue());
                    }
                }
                if (mUploadDataStream != null) {
                    if (!hasContentType) {
                        throw new IllegalArgumentException(
                                "Requests with upload data must have a Content-Type.");
                    }
                    mStarted = true;
                    mUploadDataStream.postTaskToExecutor(new Runnable() {
                        @Override
                        public void run() {
                            mUploadDataStream.initializeWithRequest();
                            synchronized (mUrlRequestAdapterLock) {
                                if (isDoneLocked()) {
                                    return;
                                }
                                mUploadDataStream.attachNativeAdapterToRequest(mUrlRequestAdapter);
                                startInternalLocked();
                            }
                        }
                    });
                    return;
                }
            } catch (RuntimeException e) {
                // If there's an exception, cleanup and then throw the exception to the caller.
                // start() is synchronized so we do not acquire mUrlRequestAdapterLock here.
                destroyRequestAdapterLocked(RequestFinishedInfo.FAILED);
                throw e;
            }
            mStarted = true;
            startInternalLocked();
        }
    }

    /*
     * Starts fully configured request. Could execute on UploadDataProvider executor.
     * Caller is expected to ensure that request isn't canceled and mUrlRequestAdapter is valid.
     */
    @GuardedBy("mUrlRequestAdapterLock")
    private void startInternalLocked() {
        CronetUrlRequestJni.get().start(mUrlRequestAdapter, CronetUrlRequest.this);
    }

    @Override
    public void followRedirect() {
        synchronized (mUrlRequestAdapterLock) {
            if (!mWaitingOnRedirect) {
                throw new IllegalStateException("No redirect to follow.");
            }
            mWaitingOnRedirect = false;

            if (isDoneLocked()) {
                return;
            }

            CronetUrlRequestJni.get().followDeferredRedirect(
                    mUrlRequestAdapter, CronetUrlRequest.this);
        }
    }

    @Override
    public void read(ByteBuffer buffer) {
        Preconditions.checkHasRemaining(buffer);
        Preconditions.checkDirect(buffer);
        synchronized (mUrlRequestAdapterLock) {
            if (!mWaitingOnRead) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            mWaitingOnRead = false;

            if (isDoneLocked()) {
                return;
            }

            if (!CronetUrlRequestJni.get().readData(mUrlRequestAdapter, CronetUrlRequest.this,
                        buffer, buffer.position(), buffer.limit())) {
                // Still waiting on read. This is just to have consistent
                // behavior with the other error cases.
                mWaitingOnRead = true;
                throw new IllegalArgumentException("Unable to call native read");
            }
        }
    }

    @Override
    public void cancel() {
        synchronized (mUrlRequestAdapterLock) {
            if (isDoneLocked() || !mStarted) {
                return;
            }
            destroyRequestAdapterLocked(RequestFinishedInfo.CANCELED);
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mUrlRequestAdapterLock) {
            return isDoneLocked();
        }
    }

    @GuardedBy("mUrlRequestAdapterLock")
    private boolean isDoneLocked() {
        return mStarted && mUrlRequestAdapter == 0;
    }

    @Override
    public void getStatus(UrlRequest.StatusListener unsafeListener) {
        final VersionSafeCallbacks.UrlRequestStatusListener listener =
                new VersionSafeCallbacks.UrlRequestStatusListener(unsafeListener);
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter != 0) {
                CronetUrlRequestJni.get().getStatus(
                        mUrlRequestAdapter, CronetUrlRequest.this, listener);
                return;
            }
        }
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(UrlRequest.Status.INVALID);
            }
        };
        postTaskToExecutor(task);
    }

    @VisibleForTesting
    public void setOnDestroyedCallbackForTesting(Runnable onDestroyedCallbackForTesting) {
        synchronized (mUrlRequestAdapterLock) {
            mOnDestroyedCallbackForTesting = onDestroyedCallbackForTesting;
        }
    }

    @VisibleForTesting
    public void setOnDestroyedUploadCallbackForTesting(
            Runnable onDestroyedUploadCallbackForTesting) {
        mUploadDataStream.setOnDestroyedCallbackForTesting(onDestroyedUploadCallbackForTesting);
    }

    @VisibleForTesting
    public long getUrlRequestAdapterForTesting() {
        synchronized (mUrlRequestAdapterLock) {
            return mUrlRequestAdapter;
        }
    }

    /**
     * Posts task to application Executor. Used for Listener callbacks
     * and other tasks that should not be executed on network thread.
     */
    private void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
            // If posting a task throws an exception, then we fail the request. This exception could
            // be permanent (executor shutdown), transient (AbortPolicy, or CallerRunsPolicy with
            // direct execution not permitted), or caused by the runnables we submit if
            // mUserExecutor is a direct executor and direct execution is not permitted. In the
            // latter two cases, there is at least have a chance to inform the embedder of the
            // request's failure, since failWithException does not enforce that onFailed() is not
            // executed inline.
            failWithException(
                    new CronetExceptionImpl("Exception posting task to executor", failException));
        }
    }

    private static int convertRequestPriority(int priority) {
        switch (priority) {
            case Builder.REQUEST_PRIORITY_IDLE:
                return RequestPriority.IDLE;
            case Builder.REQUEST_PRIORITY_LOWEST:
                return RequestPriority.LOWEST;
            case Builder.REQUEST_PRIORITY_LOW:
                return RequestPriority.LOW;
            case Builder.REQUEST_PRIORITY_MEDIUM:
                return RequestPriority.MEDIUM;
            case Builder.REQUEST_PRIORITY_HIGHEST:
                return RequestPriority.HIGHEST;
            default:
                return RequestPriority.MEDIUM;
        }
    }

    private UrlResponseInfoImpl prepareResponseInfoOnNetworkThread(int httpStatusCode,
            String httpStatusText, String[] headers, boolean wasCached, String negotiatedProtocol,
            String proxyServer, long receivedByteCount) {
        HeadersList headersList = new HeadersList();
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                    headers[i], headers[i + 1]));
        }
        return new UrlResponseInfoImpl(new ArrayList<String>(mUrlChain), httpStatusCode,
                httpStatusText, headersList, wasCached, negotiatedProtocol, proxyServer,
                receivedByteCount);
    }

    private void checkNotStarted() {
        synchronized (mUrlRequestAdapterLock) {
            if (mStarted || isDoneLocked()) {
                throw new IllegalStateException("Request is already started.");
            }
        }
    }

    /**
     * Helper method to set final status of CronetUrlRequest and clean up the
     * native request adapter.
     */
    @GuardedBy("mUrlRequestAdapterLock")
    private void destroyRequestAdapterLocked(
            @RequestFinishedInfoImpl.FinishedReason int finishedReason) {
        assert mException == null || finishedReason == RequestFinishedInfo.FAILED;
        mFinishedReason = finishedReason;
        if (mUrlRequestAdapter == 0) {
            return;
        }
        mRequestContext.onRequestDestroyed();
        // Posts a task to destroy the native adapter.
        CronetUrlRequestJni.get().destroy(mUrlRequestAdapter, CronetUrlRequest.this,
                finishedReason == RequestFinishedInfo.CANCELED);
        mUrlRequestAdapter = 0;
    }

    /**
     * If callback method throws an exception, request gets canceled
     * and exception is reported via onFailed listener callback.
     * Only called on the Executor.
     */
    private void onCallbackException(Exception e) {
        CallbackException requestError =
                new CallbackExceptionImpl("Exception received from UrlRequest.Callback", e);
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in CalledByNative method", e);
        failWithException(requestError);
    }

    /**
     * Called when UploadDataProvider encounters an error.
     */
    void onUploadException(Throwable e) {
        CallbackException uploadError =
                new CallbackExceptionImpl("Exception received from UploadDataProvider", e);
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in upload method", e);
        failWithException(uploadError);
    }

    /**
     * Fails the request with an exception on any thread.
     */
    private void failWithException(final CronetException exception) {
        synchronized (mUrlRequestAdapterLock) {
            if (isDoneLocked()) {
                return;
            }
            assert mException == null;
            mException = exception;
            destroyRequestAdapterLocked(RequestFinishedInfo.FAILED);
        }
        // onFailed will be invoked from onNativeAdapterDestroyed() to ensure metrics collection.
    }

    ////////////////////////////////////////////////
    // Private methods called by the native code.
    // Always called on network thread.
    ////////////////////////////////////////////////

    /**
     * Called before following redirects. The redirect will only be followed if
     * {@link #followRedirect()} is called. If the redirect response has a body, it will be ignored.
     * This will only be called between start and onResponseStarted.
     *
     * @param newLocation Location where request is redirected.
     * @param httpStatusCode from redirect response
     * @param receivedByteCount count of bytes received for redirect response
     * @param headers an array of response headers with keys at the even indices
     *         followed by the corresponding values at the odd indices.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onRedirectReceived(final String newLocation, int httpStatusCode,
            String httpStatusText, String[] headers, boolean wasCached, String negotiatedProtocol,
            String proxyServer, long receivedByteCount) {
        final UrlResponseInfoImpl responseInfo =
                prepareResponseInfoOnNetworkThread(httpStatusCode, httpStatusText, headers,
                        wasCached, negotiatedProtocol, proxyServer, receivedByteCount);

        // Have to do this after creating responseInfo.
        mUrlChain.add(newLocation);

        Runnable task = new Runnable() {
            @Override
            public void run() {
                checkCallingThread();
                synchronized (mUrlRequestAdapterLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    mWaitingOnRedirect = true;
                }

                try {
                    mCallback.onRedirectReceived(CronetUrlRequest.this, responseInfo, newLocation);
                } catch (Exception e) {
                    onCallbackException(e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each request.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseStarted(int httpStatusCode, String httpStatusText, String[] headers,
            boolean wasCached, String negotiatedProtocol, String proxyServer,
            long receivedByteCount) {
        mResponseInfo = prepareResponseInfoOnNetworkThread(httpStatusCode, httpStatusText, headers,
                wasCached, negotiatedProtocol, proxyServer, receivedByteCount);
        Runnable task = new Runnable() {
            @Override
            public void run() {
                checkCallingThread();
                synchronized (mUrlRequestAdapterLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    mWaitingOnRead = true;
                }

                try {
                    mCallback.onResponseStarted(CronetUrlRequest.this, mResponseInfo);
                } catch (Exception e) {
                    onCallbackException(e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called whenever data is received. The ByteBuffer remains
     * valid only until listener callback. Or if the callback
     * pauses the request, it remains valid until the request is resumed.
     * Cancelling the request also invalidates the buffer.
     *
     * @param byteBuffer ByteBuffer containing received data, starting at
     *        initialPosition. Guaranteed to have at least one read byte. Its
     *        limit has not yet been updated to reflect the bytes read.
     * @param bytesRead Number of bytes read.
     * @param initialPosition Original position of byteBuffer when passed to
     *        read(). Used as a minimal check that the buffer hasn't been
     *        modified while reading from the network.
     * @param initialLimit Original limit of byteBuffer when passed to
     *        read(). Used as a minimal check that the buffer hasn't been
     *        modified while reading from the network.
     * @param receivedByteCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onReadCompleted(final ByteBuffer byteBuffer, int bytesRead, int initialPosition,
            int initialLimit, long receivedByteCount) {
        mResponseInfo.setReceivedByteCount(receivedByteCount);
        if (byteBuffer.position() != initialPosition || byteBuffer.limit() != initialLimit) {
            failWithException(
                    new CronetExceptionImpl("ByteBuffer modified externally during read", null));
            return;
        }
        if (mOnReadCompletedTask == null) {
            mOnReadCompletedTask = new OnReadCompletedRunnable();
        }
        byteBuffer.position(initialPosition + bytesRead);
        mOnReadCompletedTask.mByteBuffer = byteBuffer;
        postTaskToExecutor(mOnReadCompletedTask);
    }

    /**
     * Called when request is completed successfully, no callbacks will be
     * called afterwards.
     *
     * @param receivedByteCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onSucceeded(long receivedByteCount) {
        mResponseInfo.setReceivedByteCount(receivedByteCount);
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    // Destroy adapter first, so request context could be shut
                    // down from the listener.
                    destroyRequestAdapterLocked(RequestFinishedInfo.SUCCEEDED);
                }
                try {
                    mCallback.onSucceeded(CronetUrlRequest.this, mResponseInfo);
                    maybeReportMetrics();
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onSucceeded method", e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called when error has occurred, no callbacks will be called afterwards.
     *
     * @param errorCode Error code represented by {@code UrlRequestError} that should be mapped
     *                  to one of {@link NetworkException#ERROR_HOSTNAME_NOT_RESOLVED
     *                  NetworkException.ERROR_*}.
     * @param nativeError native net error code.
     * @param errorString textual representation of the error code.
     * @param receivedByteCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onError(int errorCode, int nativeError, int nativeQuicError, String errorString,
            long receivedByteCount) {
        if (mResponseInfo != null) {
            mResponseInfo.setReceivedByteCount(receivedByteCount);
        }
        if (errorCode == NetworkException.ERROR_QUIC_PROTOCOL_FAILED
                || errorCode == NetworkException.ERROR_NETWORK_CHANGED) {
            failWithException(new QuicExceptionImpl("Exception in CronetUrlRequest: " + errorString,
                    errorCode, nativeError, nativeQuicError));
        } else {
            int javaError = mapUrlRequestErrorToApiErrorCode(errorCode);
            failWithException(new NetworkExceptionImpl(
                    "Exception in CronetUrlRequest: " + errorString, javaError, nativeError));
        }
    }

    /**
     * Called when request is canceled, no callbacks will be called afterwards.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onCanceled() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                try {
                    mCallback.onCanceled(CronetUrlRequest.this, mResponseInfo);
                    maybeReportMetrics();
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onCanceled method", e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called by the native code when request status is fetched from the
     * native stack.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onStatus(
            final VersionSafeCallbacks.UrlRequestStatusListener listener, final int loadState) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(convertLoadState(loadState));
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called by the native code on the network thread to report metrics. Happens before
     * onSucceeded, onError and onCanceled.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onMetricsCollected(long requestStartMs, long dnsStartMs, long dnsEndMs,
            long connectStartMs, long connectEndMs, long sslStartMs, long sslEndMs,
            long sendingStartMs, long sendingEndMs, long pushStartMs, long pushEndMs,
            long responseStartMs, long requestEndMs, boolean socketReused, long sentByteCount,
            long receivedByteCount) {
        synchronized (mUrlRequestAdapterLock) {
            if (mMetrics != null) {
                throw new IllegalStateException("Metrics collection should only happen once.");
            }
            mMetrics = new CronetMetrics(requestStartMs, dnsStartMs, dnsEndMs, connectStartMs,
                    connectEndMs, sslStartMs, sslEndMs, sendingStartMs, sendingEndMs, pushStartMs,
                    pushEndMs, responseStartMs, requestEndMs, socketReused, sentByteCount,
                    receivedByteCount);
        }
        // Metrics are reported to RequestFinishedListener when the final UrlRequest.Callback has
        // been invoked.
    }

    /**
     * Called when the native adapter is destroyed.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onNativeAdapterDestroyed() {
        synchronized (mUrlRequestAdapterLock) {
            if (mOnDestroyedCallbackForTesting != null) {
                mOnDestroyedCallbackForTesting.run();
            }
            // mException is set when an error is encountered (in native code via onError or in
            // Java code). If mException is not null, notify the mCallback and report metrics.
            if (mException == null) {
                return;
            }
        }
        Runnable task = new Runnable() {
            @Override
            public void run() {
                try {
                    mCallback.onFailed(CronetUrlRequest.this, mResponseInfo, mException);
                    maybeReportMetrics();
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onFailed method", e);
                }
            }
        };
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException e) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor", e);
        }
    }

    /** Enforces prohibition of direct execution. */
    void checkCallingThread() {
        if (!mAllowDirectExecutor && mRequestContext.isNetworkThread(Thread.currentThread())) {
            throw new InlineExecutionProhibitedException();
        }
    }

    private int mapUrlRequestErrorToApiErrorCode(int errorCode) {
        switch (errorCode) {
            case UrlRequestError.HOSTNAME_NOT_RESOLVED:
                return NetworkException.ERROR_HOSTNAME_NOT_RESOLVED;
            case UrlRequestError.INTERNET_DISCONNECTED:
                return NetworkException.ERROR_INTERNET_DISCONNECTED;
            case UrlRequestError.NETWORK_CHANGED:
                return NetworkException.ERROR_NETWORK_CHANGED;
            case UrlRequestError.TIMED_OUT:
                return NetworkException.ERROR_TIMED_OUT;
            case UrlRequestError.CONNECTION_CLOSED:
                return NetworkException.ERROR_CONNECTION_CLOSED;
            case UrlRequestError.CONNECTION_TIMED_OUT:
                return NetworkException.ERROR_CONNECTION_TIMED_OUT;
            case UrlRequestError.CONNECTION_REFUSED:
                return NetworkException.ERROR_CONNECTION_REFUSED;
            case UrlRequestError.CONNECTION_RESET:
                return NetworkException.ERROR_CONNECTION_RESET;
            case UrlRequestError.ADDRESS_UNREACHABLE:
                return NetworkException.ERROR_ADDRESS_UNREACHABLE;
            case UrlRequestError.QUIC_PROTOCOL_FAILED:
                return NetworkException.ERROR_QUIC_PROTOCOL_FAILED;
            case UrlRequestError.OTHER:
                return NetworkException.ERROR_OTHER;
            default:
                Log.e(CronetUrlRequestContext.LOG_TAG, "Unknown error code: " + errorCode);
                return errorCode;
        }
    }

    // Maybe report metrics. This method should only be called on Callback's executor thread and
    // after Callback's onSucceeded, onFailed and onCanceled.
    private void maybeReportMetrics() {
        if (mMetrics != null) {
            final RequestFinishedInfo requestInfo = new RequestFinishedInfoImpl(mInitialUrl,
                    mRequestAnnotations, mMetrics, mFinishedReason, mResponseInfo, mException);
            mRequestContext.reportRequestFinished(requestInfo);
            if (mRequestFinishedListener != null) {
                try {
                    mRequestFinishedListener.getExecutor().execute(new Runnable() {
                        @Override
                        public void run() {
                            mRequestFinishedListener.onRequestFinished(requestInfo);
                        }
                    });
                } catch (RejectedExecutionException failException) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                            failException);
                }
            }
        }
    }

    // Native methods are implemented in cronet_url_request_adapter.cc.
    @NativeMethods
    interface Natives {
        long createRequestAdapter(CronetUrlRequest caller, long urlRequestContextAdapter,
                String url, int priority, boolean disableCache, boolean disableConnectionMigration,
                boolean enableMetrics, boolean trafficStatsTagSet, int trafficStatsTag,
                boolean trafficStatsUidSet, int trafficStatsUid);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        boolean setHttpMethod(long nativePtr, CronetUrlRequest caller, String method);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        boolean addRequestHeader(
                long nativePtr, CronetUrlRequest caller, String name, String value);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        void start(long nativePtr, CronetUrlRequest caller);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        void followDeferredRedirect(long nativePtr, CronetUrlRequest caller);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        boolean readData(long nativePtr, CronetUrlRequest caller, ByteBuffer byteBuffer,
                int position, int capacity);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        void destroy(long nativePtr, CronetUrlRequest caller, boolean sendOnCanceled);

        @NativeClassQualifiedName("CronetURLRequestAdapter")
        void getStatus(long nativePtr, CronetUrlRequest caller,
                VersionSafeCallbacks.UrlRequestStatusListener listener);
    }
}
