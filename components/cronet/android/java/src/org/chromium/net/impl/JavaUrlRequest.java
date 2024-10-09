// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.TrafficStats;
import android.os.Build;
import android.os.Process;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.ConnectionCloseSource;
import org.chromium.net.CronetException;
import org.chromium.net.ExperimentalUrlRequest;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.NetworkException;
import org.chromium.net.ThreadStatsUid;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.JavaUrlRequestUtils.CheckedRunnable;
import org.chromium.net.impl.JavaUrlRequestUtils.DirectPreventingExecutor;
import org.chromium.net.impl.JavaUrlRequestUtils.State;
import org.chromium.net.impl.VersionSafeCallbacks.UploadDataProviderWrapper;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.WritableByteChannel;
import java.time.Duration;
import java.util.AbstractMap.SimpleEntry;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/** Pure java UrlRequest, backed by {@link HttpURLConnection}. */
final class JavaUrlRequest extends ExperimentalUrlRequest {
    private static final String X_ANDROID = "X-Android";
    private static final String X_ANDROID_SELECTED_TRANSPORT = "X-Android-Selected-Transport";
    private static final String TAG = JavaUrlRequest.class.getSimpleName();
    private static final int DEFAULT_CHUNK_LENGTH =
            JavaUploadDataSinkBase.DEFAULT_UPLOAD_BUFFER_SIZE;
    private static final String USER_AGENT = "User-Agent";
    private final AsyncUrlRequestCallback mCallbackAsync;
    private final Executor mExecutor;
    private final String mUserAgent;
    private final Map<String, String> mRequestHeaders =
            new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
    private final List<String> mUrlChain = new ArrayList<>();

    /**
     * This is the source of thread safety in this class - no other synchronization is performed. By
     * compare-and-swapping from one state to another, we guarantee that operations aren't running
     * concurrently. Only the winner of a CAS proceeds.
     *
     * <p>A caller can lose a CAS for three reasons - user error (two calls to read() without
     * waiting for the read to succeed), runtime error (network code or user code throws an
     * exception), or cancellation.
     */
    private final AtomicInteger /* State */ mState = new AtomicInteger(State.NOT_STARTED);

    private final AtomicBoolean mUploadProviderClosed = new AtomicBoolean(false);

    private final boolean mAllowDirectExecutor;

    /* These don't change with redirects */
    private final String mInitialMethod;
    private VersionSafeCallbacks.UploadDataProviderWrapper mUploadDataProvider;
    private Executor mUploadExecutor;

    /**
     * Holds a subset of StatusValues - {@link State#STARTED} can represent {@link
     * Status#SENDING_REQUEST} or {@link Status#WAITING_FOR_RESPONSE}. While the distinction isn't
     * needed to implement the logic in this class, it is needed to implement {@link
     * #getStatus(StatusListener)}.
     *
     * <p>Concurrency notes - this value is not atomically updated with mState, so there is some
     * risk that we'd get an inconsistent snapshot of both - however, it also happens that this
     * value is only used with the STARTED state, so it's inconsequential.
     */
    @UrlRequestUtil.StatusValues private volatile int mAdditionalStatusDetails = Status.INVALID;

    /* These change with redirects. */
    private String mCurrentUrl;
    @Nullable private ReadableByteChannel mResponseChannel; // Only accessed on mExecutor.
    private UrlResponseInfoImpl mUrlResponseInfo;
    private String mPendingRedirectUrl;
    private HttpURLConnection mCurrentUrlConnection; // Only accessed on mExecutor.
    private OutputStreamDataSink mOutputStreamDataSink; // Only accessed on mExecutor.
    private final JavaCronetEngine mEngine;
    private final int mCronetEngineId;
    private final CronetLogger mLogger;

    private final long mNetworkHandle;

    private int mReadCount;
    private int mNonfinalUserCallbackExceptionCount;
    private boolean mFinalUserCallbackThrew;

    // Executor that runs one task at a time on an underlying Executor.
    // NOTE: Do not use to wrap user supplied Executor as lock is held while underlying execute()
    // is called.
    private static final class SerializingExecutor implements Executor {
        private final Executor mUnderlyingExecutor;
        private final Runnable mRunTasks = this::runTasks;

        // Queue of tasks to run.  Tasks are added to the end and taken from the front.
        // Synchronized on itself.
        @GuardedBy("mTaskQueue")
        private final ArrayDeque<Runnable> mTaskQueue = new ArrayDeque<>();

        // Indicates if mRunTasks is actively running tasks.  Synchronized on mTaskQueue.
        @GuardedBy("mTaskQueue")
        private boolean mRunning;

        SerializingExecutor(Executor underlyingExecutor) {
            mUnderlyingExecutor = underlyingExecutor;
        }

        @Override
        public void execute(Runnable command) {
            synchronized (mTaskQueue) {
                mTaskQueue.addLast(command);
                try {
                    mUnderlyingExecutor.execute(mRunTasks);
                } catch (RejectedExecutionException e) {
                    // If shutting down, do not add new tasks to the queue.
                    mTaskQueue.removeLast();
                }
            }
        }

        private void runTasks() {
            Runnable task;
            synchronized (mTaskQueue) {
                if (mRunning) {
                    return;
                }
                task = mTaskQueue.pollFirst();
                mRunning = task != null;
            }
            while (task != null) {
                boolean threw = true;
                try {
                    task.run();
                    threw = false;
                } finally {
                    synchronized (mTaskQueue) {
                        if (threw) {
                            // If task.run() threw, this method will abort without
                            // looping again, so repost to keep running tasks.
                            mRunning = false;
                            try {
                                mUnderlyingExecutor.execute(mRunTasks);
                            } catch (RejectedExecutionException e) {
                                // Give up if a task run at shutdown throws.
                            }
                        } else {
                            task = mTaskQueue.pollFirst();
                            mRunning = task != null;
                        }
                    }
                }
            }
        }
    }

    /**
     * @param executor The executor used for reading and writing from sockets
     * @param userExecutor The executor used to dispatch to {@code callback}
     */
    JavaUrlRequest(
            JavaCronetEngine engine,
            UrlRequest.Callback callback,
            final Executor executor,
            Executor userExecutor,
            String url,
            String userAgent,
            boolean allowDirectExecutor,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            final boolean trafficStatsUidSet,
            final int trafficStatsUid,
            long networkHandle,
            String method,
            ArrayList<Map.Entry<String, String>> requestHeaders,
            UploadDataProvider uploadDataProvider,
            Executor uploadDataProviderExecutor) {
        Objects.requireNonNull(url, "URL is required");
        Objects.requireNonNull(callback, "Listener is required");
        Objects.requireNonNull(executor, "Executor is required");
        Objects.requireNonNull(userExecutor, "userExecutor is required");

        mAllowDirectExecutor = allowDirectExecutor;
        mCallbackAsync = new AsyncUrlRequestCallback(callback, userExecutor);
        final int trafficStatsTagToUse =
                trafficStatsTagSet ? trafficStatsTag : TrafficStats.getThreadStatsTag();
        mExecutor =
                new SerializingExecutor(
                        (command) -> {
                            executor.execute(
                                    () -> {
                                        int oldTag = TrafficStats.getThreadStatsTag();
                                        TrafficStats.setThreadStatsTag(trafficStatsTagToUse);
                                        if (trafficStatsUidSet) {
                                            ThreadStatsUid.set(trafficStatsUid);
                                        }
                                        try {
                                            command.run();
                                        } finally {
                                            if (trafficStatsUidSet) {
                                                ThreadStatsUid.clear();
                                            }
                                            TrafficStats.setThreadStatsTag(oldTag);
                                        }
                                    });
                        });
        mEngine = engine;
        mCronetEngineId = engine.getCronetEngineId();
        mLogger = engine.getCronetLogger();
        mCurrentUrl = url;
        mUserAgent = userAgent;
        mNetworkHandle = networkHandle;
        mInitialMethod = checkedHttpMethod(method);
        setHeaders(requestHeaders);
        mUploadDataProvider = checkedUploadDataProvider(uploadDataProvider);
        mUploadExecutor =
                uploadDataProviderExecutor == null || mAllowDirectExecutor
                        ? uploadDataProviderExecutor
                        : new DirectPreventingExecutor(uploadDataProviderExecutor);
    }

    private static String checkedHttpMethod(String method) {
        Objects.requireNonNull(method, "Method is required.");
        if ("OPTIONS".equalsIgnoreCase(method)
                || "GET".equalsIgnoreCase(method)
                || "HEAD".equalsIgnoreCase(method)
                || "POST".equalsIgnoreCase(method)
                || "PUT".equalsIgnoreCase(method)
                || "DELETE".equalsIgnoreCase(method)
                || "TRACE".equalsIgnoreCase(method)
                || "PATCH".equalsIgnoreCase(method)) {
            return method;
        } else {
            throw new IllegalArgumentException("Invalid http method " + method);
        }
    }

    private void setHeaders(ArrayList<Map.Entry<String, String>> requestHeaders) {
        for (Map.Entry<String, String> header : requestHeaders) {
            if (!isValidHeaderName(header.getKey()) || header.getValue().contains("\r\n")) {
                throw new IllegalArgumentException(
                        "Invalid header with headername: " + header.getKey());
            }
            mRequestHeaders.put(header.getKey(), header.getValue());
        }
    }

    private static boolean isValidHeaderName(String header) {
        for (int i = 0; i < header.length(); i++) {
            char c = header.charAt(i);
            switch (c) {
                case '(':
                case ')':
                case '<':
                case '>':
                case '@':
                case ',':
                case ';':
                case ':':
                case '\\':
                case '\'':
                case '/':
                case '[':
                case ']':
                case '?':
                case '=':
                case '{':
                case '}':
                    return false;
                default:
                    if (Character.isISOControl(c) || Character.isWhitespace(c)) {
                        return false;
                    }
            }
        }
        return true;
    }

    private UploadDataProviderWrapper checkedUploadDataProvider(
            UploadDataProvider uploadDataProvider) {
        if (uploadDataProvider == null) {
            return null;
        }

        if (!mRequestHeaders.containsKey("Content-Type")) {
            throw new IllegalArgumentException(
                    "Requests with upload data must have a Content-Type.");
        }
        return new VersionSafeCallbacks.UploadDataProviderWrapper(uploadDataProvider);
    }

    private final class OutputStreamDataSink extends JavaUploadDataSinkBase {
        private final HttpURLConnection mUrlConnection;
        private final AtomicBoolean mOutputChannelClosed = new AtomicBoolean(false);
        private WritableByteChannel mOutputChannel;
        private OutputStream mUrlConnectionOutputStream;

        OutputStreamDataSink(
                final Executor userExecutor,
                Executor executor,
                HttpURLConnection urlConnection,
                VersionSafeCallbacks.UploadDataProviderWrapper provider) {
            super(userExecutor, executor, provider);
            mUrlConnection = urlConnection;
        }

        @Override
        protected void initializeRead() throws IOException {
            if (mOutputChannel == null) {
                mAdditionalStatusDetails = Status.CONNECTING;
                mUrlConnection.setDoOutput(true);
                mUrlConnection.connect();
                mAdditionalStatusDetails = Status.SENDING_REQUEST;
                mUrlConnectionOutputStream = mUrlConnection.getOutputStream();
                mOutputChannel = Channels.newChannel(mUrlConnectionOutputStream);
            }
        }

        void closeOutputChannel() throws IOException {
            if (mOutputChannel != null
                    && mOutputChannelClosed.compareAndSet(
                            /* expected= */ false, /* updated= */ true)) {
                mOutputChannel.close();
            }
        }

        @Override
        protected void finish() throws IOException {
            closeOutputChannel();
            fireGetHeaders();
        }

        @Override
        protected void initializeStart(long totalBytes) {
            if (totalBytes > 0) {
                mUrlConnection.setFixedLengthStreamingMode(totalBytes);
            } else {
                mUrlConnection.setChunkedStreamingMode(DEFAULT_CHUNK_LENGTH);
            }
        }

        @Override
        protected int processSuccessfulRead(ByteBuffer buffer) throws IOException {
            int totalBytesProcessed = 0;
            while (buffer.hasRemaining()) {
                totalBytesProcessed += mOutputChannel.write(buffer);
            }
            // Forces a chunk to be sent, rather than buffering to the DEFAULT_CHUNK_LENGTH.
            // This allows clients to trickle-upload bytes as they become available without
            // introducing latency due to buffering.
            mUrlConnectionOutputStream.flush();
            return totalBytesProcessed;
        }

        @Override
        protected Runnable getErrorSettingRunnable(CheckedRunnable runnable) {
            return errorSetting(runnable);
        }

        @Override
        protected Runnable getUploadErrorSettingRunnable(CheckedRunnable runnable) {
            return uploadErrorSetting(runnable);
        }

        @Override
        protected void processUploadError(Throwable exception) {
            enterUploadErrorState(exception);
        }
    }

    @Override
    public void start() {
        mAdditionalStatusDetails = Status.CONNECTING;
        mEngine.incrementActiveRequestCount();
        transitionStates(
                State.NOT_STARTED,
                State.STARTED,
                () -> {
                    mUrlChain.add(mCurrentUrl);
                    fireOpenConnection();
                });
    }

    private void enterErrorState(final CronetException error) {
        if (setTerminalState(State.ERROR)) {
            fireDisconnect();
            fireCloseUploadDataProvider();
            mCallbackAsync.onFailed(mUrlResponseInfo, error);
        }
    }

    private boolean setTerminalState(@State int error) {
        while (true) {
            @State int oldState = mState.get();
            switch (oldState) {
                case State.NOT_STARTED:
                    throw new IllegalStateException("Can't enter error state before start");
                case State.ERROR: // fallthrough
                case State.COMPLETE: // fallthrough
                case State.CANCELLED:
                    return false; // Already in a terminal state
                default:
                    if (mState.compareAndSet(/* expected= */ oldState, /* updated= */ error)) {
                        return true;
                    }
            }
        }
    }

    /** Ends the request with an error, caused by an exception thrown from user code. */
    private void enterUserErrorState(final Throwable error) {
        // We schedule this on the internal executor, which is serialized, to ensure we don't race
        // against the read code path (which can be scheduled on the internal executor at any time,
        // e.g. through onCanceled).
        //
        // It's still possible that a non-final user callback may throw an exception after the
        // terminal callback returned and we already logged this metric, in which case we will miss
        // the exception. Arguably this is too unlikely for us to care.
        mExecutor.execute(
                () -> {
                    mNonfinalUserCallbackExceptionCount++;
                });

        enterErrorState(
                new CallbackExceptionImpl("Exception received from UrlRequest.Callback", error));
    }

    /** Ends the request with an error, caused by an exception thrown from user code. */
    private void enterUploadErrorState(final Throwable error) {
        enterErrorState(
                new CallbackExceptionImpl("Exception received from UploadDataProvider", error));
    }

    private void enterCronetErrorState(final Throwable error) {
        // TODO(clm) mapping from Java exception (UnknownHostException, for example) to net error
        // code goes here.
        enterErrorState(new CronetExceptionImpl("System error", error));
    }

    /**
     * Atomically swaps from the expected state to a new state. If the swap fails, and it's not
     * due to an earlier error or cancellation, throws an exception.
     *
     * @param afterTransition Callback to run after transition completes successfully.
     */
    private void transitionStates(
            @State int expected, @State int newState, Runnable afterTransition) {
        if (!mState.compareAndSet(expected, newState)) {
            @State int state = mState.get();
            if (!(state == State.CANCELLED || state == State.ERROR)) {
                throw new IllegalStateException(
                        "Invalid state transition - expected " + expected + " but was " + state);
            }
        } else {
            afterTransition.run();
        }
    }

    @Override
    public void followRedirect() {
        transitionStates(
                State.AWAITING_FOLLOW_REDIRECT,
                State.STARTED,
                new Runnable() {
                    @Override
                    public void run() {
                        mCurrentUrl = mPendingRedirectUrl;
                        mPendingRedirectUrl = null;
                        fireOpenConnection();
                    }
                });
    }

    private void fireGetHeaders() {
        mAdditionalStatusDetails = Status.WAITING_FOR_RESPONSE;
        mExecutor.execute(
                errorSetting(
                        () -> {
                            if (mCurrentUrlConnection == null) {
                                return; // We've been cancelled
                            }
                            final List<Map.Entry<String, String>> headerList = new ArrayList<>();
                            String selectedTransport = "http/1.1";
                            String headerKey;
                            for (int i = 0;
                                    (headerKey = mCurrentUrlConnection.getHeaderFieldKey(i))
                                            != null;
                                    i++) {
                                if (X_ANDROID_SELECTED_TRANSPORT.equalsIgnoreCase(headerKey)) {
                                    selectedTransport = mCurrentUrlConnection.getHeaderField(i);
                                }
                                if (!headerKey.startsWith(X_ANDROID)) {
                                    headerList.add(
                                            new SimpleEntry<>(
                                                    headerKey,
                                                    mCurrentUrlConnection.getHeaderField(i)));
                                }
                            }

                            int responseCode = mCurrentUrlConnection.getResponseCode();
                            // Important to copy mUrlChain here, because although we never
                            // concurrently modify mUrlChain ourselves, user code might iterate
                            // over it while we're redirecting, and that would throw
                            // ConcurrentModificationException.
                            UrlResponseInfoImpl responseInfo =
                                    new UrlResponseInfoImpl(
                                            new ArrayList<>(mUrlChain),
                                            responseCode,
                                            mCurrentUrlConnection.getResponseMessage(),
                                            Collections.unmodifiableList(headerList),
                                            false,
                                            selectedTransport,
                                            "",
                                            0);
                            // TODO(clm) actual redirect handling? post -> get and whatnot?
                            if (responseCode >= 300 && responseCode < 400) {
                                List<String> locationFields =
                                        responseInfo.getAllHeaders().get("location");
                                if (locationFields != null) {
                                    fireRedirectReceived(locationFields.get(0), responseInfo);
                                    return;
                                }
                            }
                            // Only assign mUrlResponseInfo when response is not a redirect. This
                            // aligns with CronetUrlRequest's behaviour.
                            mUrlResponseInfo = responseInfo;
                            fireCloseUploadDataProvider();
                            if (responseCode >= 400) {
                                InputStream inputStream = mCurrentUrlConnection.getErrorStream();
                                mResponseChannel =
                                        inputStream == null
                                                ? null
                                                : InputStreamChannel.wrap(inputStream);
                                mCallbackAsync.onResponseStarted(mUrlResponseInfo);
                            } else {
                                mResponseChannel =
                                        InputStreamChannel.wrap(
                                                mCurrentUrlConnection.getInputStream());
                                mCallbackAsync.onResponseStarted(mUrlResponseInfo);
                            }
                        }));
    }

    private void fireCloseUploadDataProvider() {
        if (mUploadDataProvider != null
                && mUploadProviderClosed.compareAndSet(
                        /* expected= */ false, /* updated= */ true)) {
            try {
                mUploadExecutor.execute(uploadErrorSetting(mUploadDataProvider::close));
            } catch (RejectedExecutionException e) {
                Log.e(TAG, "Exception when closing uploadDataProvider", e);
            }
        }
    }

    private void fireRedirectReceived(final String locationField, UrlResponseInfo urlResponseInfo) {
        transitionStates(
                State.STARTED,
                State.REDIRECT_RECEIVED,
                () -> {
                    mPendingRedirectUrl = URI.create(mCurrentUrl).resolve(locationField).toString();
                    mUrlChain.add(mPendingRedirectUrl);
                    transitionStates(
                            State.REDIRECT_RECEIVED,
                            State.AWAITING_FOLLOW_REDIRECT,
                            () -> {
                                mCallbackAsync.onRedirectReceived(
                                        urlResponseInfo, mPendingRedirectUrl);
                            });
                });
    }

    private void fireOpenConnection() {
        mExecutor.execute(
                errorSetting(
                        () -> {
                            // If we're cancelled, then our old connection will be disconnected
                            // for us and we shouldn't open a new one.
                            if (mState.get() == State.CANCELLED) {
                                return;
                            }

                            final URL url = new URL(mCurrentUrl);
                            if (mCurrentUrlConnection != null) {
                                mCurrentUrlConnection.disconnect();
                                mCurrentUrlConnection = null;
                            }

                            if (mNetworkHandle == CronetEngineBase.DEFAULT_NETWORK_HANDLE) {
                                mCurrentUrlConnection = (HttpURLConnection) url.openConnection();
                            } else {
                                Network network = getNetworkFromHandle(mNetworkHandle);
                                if (network == null) {
                                    throw new NetworkExceptionImpl(
                                            "Network bound to request not found",
                                            NetworkException.ERROR_ADDRESS_UNREACHABLE,
                                            -4 /*Invalid argument*/);
                                }
                                mCurrentUrlConnection =
                                        (HttpURLConnection) network.openConnection(url);
                            }
                            mCurrentUrlConnection.setInstanceFollowRedirects(false);
                            if (!mRequestHeaders.containsKey(USER_AGENT)) {
                                mRequestHeaders.put(USER_AGENT, mUserAgent);
                            }
                            for (Map.Entry<String, String> entry : mRequestHeaders.entrySet()) {
                                mCurrentUrlConnection.setRequestProperty(
                                        entry.getKey(), entry.getValue());
                            }
                            mCurrentUrlConnection.setRequestMethod(mInitialMethod);
                            if (mUploadDataProvider != null) {
                                mOutputStreamDataSink =
                                        new OutputStreamDataSink(
                                                mUploadExecutor,
                                                mExecutor,
                                                mCurrentUrlConnection,
                                                mUploadDataProvider);
                                mOutputStreamDataSink.start(mUrlChain.size() == 1);
                            } else {
                                mAdditionalStatusDetails = Status.CONNECTING;
                                mCurrentUrlConnection.connect();
                                fireGetHeaders();
                            }
                        }));
    }

    private Runnable errorSetting(final CheckedRunnable delegate) {
        return () -> {
            try {
                delegate.run();
            } catch (Throwable t) {
                enterCronetErrorState(t);
            }
        };
    }

    private Runnable userErrorSetting(final CheckedRunnable delegate) {
        return () -> {
            try {
                delegate.run();
            } catch (Throwable t) {
                enterUserErrorState(t);
            }
        };
    }

    private Runnable uploadErrorSetting(final CheckedRunnable delegate) {
        return () -> {
            try {
                delegate.run();
            } catch (Throwable t) {
                enterUploadErrorState(t);
            }
        };
    }

    @Override
    public void read(final ByteBuffer buffer) {
        Preconditions.checkDirect(buffer);
        Preconditions.checkHasRemaining(buffer);
        CheckedRunnable doRead =
                () -> {
                    int read = -1;
                    if (mResponseChannel != null) {
                        mReadCount++;
                        read = mResponseChannel.read(buffer);
                    }
                    processReadResult(read, buffer);
                };
        transitionStates(
                State.AWAITING_READ,
                State.READING,
                () -> {
                    mExecutor.execute(errorSetting(doRead));
                });
    }

    private void processReadResult(int read, final ByteBuffer buffer) throws IOException {
        if (read != -1) {
            mCallbackAsync.onReadCompleted(mUrlResponseInfo, buffer);
        } else {
            if (mResponseChannel != null) {
                mResponseChannel.close();
            }
            if (mState.compareAndSet(
                    /* expected= */ State.READING, /* updated= */ State.COMPLETE)) {
                fireDisconnect();
                mCallbackAsync.onSucceeded(mUrlResponseInfo);
            }
        }
    }

    private void fireDisconnect() {
        mExecutor.execute(
                () -> {
                    if (mOutputStreamDataSink != null) {
                        try {
                            mOutputStreamDataSink.closeOutputChannel();
                        } catch (IOException e) {
                            Log.e(TAG, "Exception when closing OutputChannel", e);
                        }
                    }
                    if (mCurrentUrlConnection != null) {
                        mCurrentUrlConnection.disconnect();
                        mCurrentUrlConnection = null;
                    }
                });
    }

    @Override
    public void cancel() {
        @State int oldState = mState.getAndSet(State.CANCELLED);
        switch (oldState) {
                // We've just scheduled some user code to run. When they perform their next
                // operation, they'll observe it and fail. However, if user code is cancelling in
                // response to one of these callbacks, we'll never actually cancel!
                // TODO(clm) figure out if it's possible to avoid concurrency in user callbacks.
            case State.REDIRECT_RECEIVED:
            case State.AWAITING_FOLLOW_REDIRECT:
            case State.AWAITING_READ:

                // User code is waiting on us - cancel away!
            case State.STARTED:
            case State.READING:
                fireDisconnect();
                fireCloseUploadDataProvider();
                mCallbackAsync.onCanceled(mUrlResponseInfo);
                break;
                // The rest are all termination cases - we're too late to cancel.
            case State.ERROR:
            case State.COMPLETE:
            case State.CANCELLED:
                break;
            default:
                break;
        }
    }

    @Override
    public boolean isDone() {
        @State int state = mState.get();
        return state == State.COMPLETE || state == State.ERROR || state == State.CANCELLED;
    }

    /**
     * Estimates the byte size of the headers in their on-wire format.
     * We are not really interested in their specific size but something which is close enough.
     */
    @VisibleForTesting
    static long estimateHeadersSizeInBytesList(Map<String, List<String>> headers) {
        if (headers == null) return 0;

        long responseHeaderSizeInBytes = 0;
        for (Map.Entry<String, List<String>> entry : headers.entrySet()) {
            String key = entry.getKey();
            if (key != null) responseHeaderSizeInBytes += key.length();
            if (entry.getValue() == null) continue;

            for (String content : entry.getValue()) {
                if (content != null) responseHeaderSizeInBytes += content.length();
            }
        }
        return responseHeaderSizeInBytes;
    }

    /**
     * Estimates the byte size of the headers in their on-wire format.
     * We are not really interested in their specific size but something which is close enough.
     */
    @VisibleForTesting
    static long estimateHeadersSizeInBytes(Map<String, String> headers) {
        if (headers == null) return 0;
        long responseHeaderSizeInBytes = 0;
        for (Map.Entry<String, String> entry : headers.entrySet()) {
            String key = entry.getKey();
            if (key != null) responseHeaderSizeInBytes += key.length();
            String value = entry.getValue();
            if (value != null) responseHeaderSizeInBytes += value.length();
        }
        return responseHeaderSizeInBytes;
    }

    private static long parseContentLengthString(String contentLength) {
        try {
            return Long.parseLong(contentLength);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    @Override
    public void getStatus(UrlRequest.StatusListener listener) {
        @State int state = mState.get();
        int extraStatus = this.mAdditionalStatusDetails;

        @UrlRequestUtil.StatusValues final int status;
        switch (state) {
            case State.ERROR:
            case State.COMPLETE:
            case State.CANCELLED:
            case State.NOT_STARTED:
                status = Status.INVALID;
                break;
            case State.STARTED:
                status = extraStatus;
                break;
            case State.REDIRECT_RECEIVED:
            case State.AWAITING_FOLLOW_REDIRECT:
            case State.AWAITING_READ:
                status = Status.IDLE;
                break;
            case State.READING:
                status = Status.READING_RESPONSE;
                break;
            default:
                throw new IllegalStateException("Switch is exhaustive: " + state);
        }

        mCallbackAsync.sendStatus(
                new VersionSafeCallbacks.UrlRequestStatusListener(listener), status);
    }

    /** This wrapper ensures that callbacks are always called on the correct executor */
    private final class AsyncUrlRequestCallback {
        final VersionSafeCallbacks.UrlRequestCallback mCallback;
        final Executor mUserExecutor;
        final Executor mFallbackExecutor;

        AsyncUrlRequestCallback(Callback callback, final Executor userExecutor) {
            this.mCallback = new VersionSafeCallbacks.UrlRequestCallback(callback);
            if (mAllowDirectExecutor) {
                this.mUserExecutor = userExecutor;
                this.mFallbackExecutor = null;
            } else {
                mUserExecutor = new DirectPreventingExecutor(userExecutor);
                mFallbackExecutor = userExecutor;
            }
        }

        void sendStatus(
                final VersionSafeCallbacks.UrlRequestStatusListener listener, final int status) {
            mUserExecutor.execute(
                    () -> {
                        listener.onStatus(status);
                    });
        }

        void execute(CheckedRunnable runnable) {
            try {
                mUserExecutor.execute(userErrorSetting(runnable));
            } catch (RejectedExecutionException e) {
                enterErrorState(new CronetExceptionImpl("Exception posting task to executor", e));
            }
        }

        void onRedirectReceived(final UrlResponseInfo info, final String newLocationUrl) {
            execute(
                    () -> {
                        mCallback.onRedirectReceived(JavaUrlRequest.this, info, newLocationUrl);
                    });
        }

        void onResponseStarted(UrlResponseInfo info) {
            execute(
                    () -> {
                        if (mState.compareAndSet(
                                /* expected= */ State.STARTED,
                                /* updated= */ State.AWAITING_READ)) {
                            mCallback.onResponseStarted(JavaUrlRequest.this, mUrlResponseInfo);
                        }
                    });
        }

        void onReadCompleted(final UrlResponseInfo info, final ByteBuffer byteBuffer) {
            execute(
                    () -> {
                        if (mState.compareAndSet(
                                /* expected= */ State.READING,
                                /* updated= */ State.AWAITING_READ)) {
                            mCallback.onReadCompleted(JavaUrlRequest.this, info, byteBuffer);
                        }
                    });
        }

        /**
         * Builds the {@link CronetTrafficInfo} associated to this request internal state. This
         * helper methods makes strong assumptions about the state of the request. For this reason
         * it should only be called within {@link JavaUrlRequest#maybeReportMetrics} where these
         * assumptions are guaranteed to be true.
         *
         * @return the {@link CronetTrafficInfo} associated to this request internal state
         */
        @RequiresApi(Build.VERSION_CODES.O)
        private CronetTrafficInfo buildCronetTrafficInfo() {
            assert mRequestHeaders != null;

            // Most of the CronetTrafficInfo fields have similar names/semantics. To avoid bugs due
            // to typos everything is final, this means that things have to initialized through an
            // if/else.
            final Map<String, List<String>> responseHeaders;
            final String negotiatedProtocol;
            final int httpStatusCode;
            final boolean wasCached;
            if (mUrlResponseInfo != null) {
                responseHeaders = mUrlResponseInfo.getAllHeaders();
                negotiatedProtocol = mUrlResponseInfo.getNegotiatedProtocol();
                httpStatusCode = mUrlResponseInfo.getHttpStatusCode();
                wasCached = mUrlResponseInfo.wasCached();
            } else {
                responseHeaders = Collections.emptyMap();
                negotiatedProtocol = "";
                httpStatusCode = 0;
                wasCached = false;
            }

            final long requestHeaderSizeInBytes;
            final long requestBodySizeInBytes;
            if (wasCached) {
                requestHeaderSizeInBytes = 0;
                requestBodySizeInBytes = 0;
            } else {
                requestHeaderSizeInBytes = estimateHeadersSizeInBytes(mRequestHeaders);
                // TODO(stefanoduo): Add logic to keep track of request body size.
                requestBodySizeInBytes = -1;
            }

            final long responseBodySizeInBytes;
            final long responseHeaderSizeInBytes;
            if (wasCached) {
                responseHeaderSizeInBytes = 0;
                responseBodySizeInBytes = 0;
            } else {
                responseHeaderSizeInBytes = estimateHeadersSizeInBytesList(responseHeaders);
                // Content-Length is not mandatory, if missing report a non-valid response body size
                // for the time being.
                if (responseHeaders.containsKey("Content-Length")) {
                    responseBodySizeInBytes =
                            parseContentLengthString(responseHeaders.get("Content-Length").get(0));
                } else {
                    // TODO(stefanoduo): Add logic to keep track of response body size.
                    responseBodySizeInBytes = -1;
                }
            }

            final Duration headersLatency = Duration.ofSeconds(0);
            final Duration totalLatency = Duration.ofSeconds(0);

            @State int state = mState.get();
            CronetTrafficInfo.RequestTerminalState requestTerminalState;
            switch (state) {
                case State.COMPLETE:
                    requestTerminalState = CronetTrafficInfo.RequestTerminalState.SUCCEEDED;
                    break;
                case State.ERROR:
                    requestTerminalState = CronetTrafficInfo.RequestTerminalState.ERROR;
                    break;
                case State.CANCELLED:
                    requestTerminalState = CronetTrafficInfo.RequestTerminalState.CANCELLED;
                    break;
                default:
                    throw new IllegalStateException(
                            "Internal Cronet error: attempted to report metrics but current state ("
                                    + state
                                    + ") is not a done state!");
            }

            return new CronetTrafficInfo(
                    requestHeaderSizeInBytes,
                    requestBodySizeInBytes,
                    responseHeaderSizeInBytes,
                    responseBodySizeInBytes,
                    httpStatusCode,
                    headersLatency,
                    totalLatency,
                    negotiatedProtocol,
                    // There is no connection migration for the fallback implementation.
                    false, // wasConnectionMigrationAttempted
                    false, // didConnectionMigrationSucceed
                    requestTerminalState,
                    mNonfinalUserCallbackExceptionCount,
                    mReadCount,
                    mOutputStreamDataSink == null ? 0 : mOutputStreamDataSink.getReadCount(),
                    /* isBidiStream= */ false,
                    mFinalUserCallbackThrew,
                    Process.myUid(),
                    /* networkInternalErrorCode */ 0,
                    /* quicErrorCode */ 0,
                    /* connectionCloseSource */ ConnectionCloseSource.UNKNOWN,
                    /* failureReason */ CronetTrafficInfo.RequestFailureReason.UNKNOWN,
                    /* socketReused */ false);
        }

        // Maybe report metrics. This method should only be called on Callback's executor thread and
        // after Callback's onSucceeded, onFailed and onCanceled.
        private void maybeReportMetrics() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

            // Schedule on the internal executor, which is serialized, to ensure we're not reading
            // data while some code running on the internal executor is still mutating it. See
            // https://crbug.com/337260115
            mExecutor.execute(
                    () -> {
                        try {
                            mLogger.logCronetTrafficInfo(mCronetEngineId, buildCronetTrafficInfo());
                        } catch (RuntimeException e) {
                            // Handle any issue gracefully, we should never crash due failures while
                            // logging.
                            Log.i(TAG, "Error while trying to log CronetTrafficInfo: ", e);
                        }
                    });
        }

        void onCanceled(final UrlResponseInfo info) {
            closeResponseChannel();
            mUserExecutor.execute(
                    () -> {
                        try {
                            mCallback.onCanceled(JavaUrlRequest.this, info);
                        } catch (Exception exception) {
                            onFinalCallbackException("onCanceled", exception);
                        }
                        maybeReportMetrics();
                        mEngine.decrementActiveRequestCount();
                    });
        }

        void onSucceeded(final UrlResponseInfo info) {
            mUserExecutor.execute(
                    () -> {
                        try {
                            mCallback.onSucceeded(JavaUrlRequest.this, info);
                        } catch (Exception exception) {
                            onFinalCallbackException("onSucceded", exception);
                        }
                        maybeReportMetrics();
                        mEngine.decrementActiveRequestCount();
                    });
        }

        void onFailed(final UrlResponseInfo urlResponseInfo, final CronetException e) {
            closeResponseChannel();
            Runnable runnable =
                    () -> {
                        try {
                            mCallback.onFailed(JavaUrlRequest.this, urlResponseInfo, e);
                        } catch (Exception exception) {
                            onFinalCallbackException("onFailed", exception);
                        }
                        maybeReportMetrics();
                        mEngine.decrementActiveRequestCount();
                    };
            try {
                mUserExecutor.execute(runnable);
            } catch (InlineExecutionProhibitedException wasDirect) {
                if (mFallbackExecutor != null) {
                    mFallbackExecutor.execute(runnable);
                }
            }
        }
    }

    private void closeResponseChannel() {
        mExecutor.execute(
                () -> {
                    if (mResponseChannel != null) {
                        try {
                            mResponseChannel.close();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                        mResponseChannel = null;
                    }
                });
    }

    private Network getNetworkFromHandle(long networkHandle) {
        Network[] networks =
                ((ConnectivityManager)
                                mEngine.getContext().getSystemService(Context.CONNECTIVITY_SERVICE))
                        .getAllNetworks();

        for (Network network : networks) {
            if (network.getNetworkHandle() == networkHandle) return network;
        }

        return null;
    }

    private void onFinalCallbackException(String method, Exception e) {
        Log.e(TAG, "Exception in " + method + " method", e);
        mFinalUserCallbackThrew = true;
    }
}
