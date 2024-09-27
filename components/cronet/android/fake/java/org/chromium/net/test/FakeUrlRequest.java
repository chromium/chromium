// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.util.Log;

import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.CronetException;
import org.chromium.net.ExperimentalUrlRequest;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.CallbackExceptionImpl;
import org.chromium.net.impl.CronetExceptionImpl;
import org.chromium.net.impl.JavaUploadDataSinkBase;
import org.chromium.net.impl.JavaUrlRequestUtils;
import org.chromium.net.impl.JavaUrlRequestUtils.CheckedRunnable;
import org.chromium.net.impl.JavaUrlRequestUtils.DirectPreventingExecutor;
import org.chromium.net.impl.JavaUrlRequestUtils.State;
import org.chromium.net.impl.Preconditions;
import org.chromium.net.impl.RefCountDelegate;
import org.chromium.net.impl.RequestFinishedInfoImpl;
import org.chromium.net.impl.UrlRequestUtil;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.URI;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

/**
 * Fake UrlRequest that retrieves responses from the associated FakeCronetController. Used for
 * testing Cronet usage on Android.
 */
final class FakeUrlRequest extends ExperimentalUrlRequest {
    // Used for logging errors.
    private static final String TAG = FakeUrlRequest.class.getSimpleName();
    // Callback used to report responses to the client.
    private final Callback mCallback;
    // The {@link Executor} provided by the user to be used for callbacks.
    private final Executor mUserExecutor;
    // The {@link Executor} provided by the engine used to break up callback loops.
    private final Executor mExecutor;
    // The Annotations provided by the engine during the creation of this request.
    private final Collection<Object> mRequestAnnotations;
    // The {@link FakeCronetController} that will provide responses for this request.
    private final FakeCronetController mFakeCronetController;
    // The fake {@link CronetEngine} that should be notified when this request starts and stops.
    private final FakeCronetEngine mFakeCronetEngine;

    // Source of thread safety for this class.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    final Object mLock = new Object();

    // True if direct execution is allowed for this request.
    private final boolean mAllowDirectExecutor;

    // The chain of URL's this request has received.
    @GuardedBy("mLock")
    private final List<String> mUrlChain = new ArrayList<>();

    // The list of HTTP headers used by this request to establish a connection.
    private final List<Map.Entry<String, String>> mAllHeadersList;

    // The exception that is thrown by the request. This is the same exception as the one in
    // onFailed
    @GuardedBy("mLock")
    private CronetException mCronetException;

    // The current URL this request is connecting to.
    @GuardedBy("mLock")
    private String mCurrentUrl;

    // The {@link FakeUrlResponse} for the current URL.
    @GuardedBy("mLock")
    private FakeUrlResponse mCurrentFakeResponse;

    // The body of the request from UploadDataProvider.
    @GuardedBy("mLock")
    private byte[] mRequestBody;

    // The {@link UploadDataProvider} to retrieve a request body from.
    private final UploadDataProvider mUploadDataProvider;

    // The executor to call the {@link UploadDataProvider}'s callback methods with.
    private final Executor mUploadExecutor;

    // The {@link UploadDataSink} for the {@link UploadDataProvider}.
    @GuardedBy("mLock")
    @VisibleForTesting
    FakeDataSink mFakeDataSink;

    // The {@link UrlResponseInfo} for the current request.
    @GuardedBy("mLock")
    private UrlResponseInfo mUrlResponseInfo;

    // The response from the current request that needs to be sent.
    @GuardedBy("mLock")
    private ByteBuffer mResponse;

    // The HTTP method used by this request to establish a connection.
    private final String mHttpMethod;

    // True after the {@link UploadDataProvider} for this request has been closed.
    @GuardedBy("mLock")
    private boolean mUploadProviderClosed;

    @GuardedBy("mLock")
    @State
    private int mState = State.NOT_STARTED;

    /**
     * Holds a subset of StatusValues - {@link State#STARTED} can represent {@link
     * Status#SENDING_REQUEST} or {@link Status#WAITING_FOR_RESPONSE}. While the distinction isn't
     * needed to implement the logic in this class, it is needed to implement {@link
     * #getStatus(StatusListener)}.
     */
    @UrlRequestUtil.StatusValues private volatile int mAdditionalStatusDetails = Status.INVALID;

    /** Used to map from HTTP status codes to the corresponding human-readable text. */
    private static final Map<Integer, String> HTTP_STATUS_CODE_TO_TEXT;

    static {
        Map<Integer, String> httpCodeMap = new HashMap<>();
        httpCodeMap.put(100, "Continue");
        httpCodeMap.put(101, "Switching Protocols");
        httpCodeMap.put(102, "Processing");
        httpCodeMap.put(103, "Early Hints");
        httpCodeMap.put(200, "OK");
        httpCodeMap.put(201, "Created");
        httpCodeMap.put(202, "Accepted");
        httpCodeMap.put(203, "Non-Authoritative Information");
        httpCodeMap.put(204, "No Content");
        httpCodeMap.put(205, "Reset Content");
        httpCodeMap.put(206, "Partial Content");
        httpCodeMap.put(207, "Multi-Status");
        httpCodeMap.put(208, "Already Reported");
        httpCodeMap.put(226, "IM Used");
        httpCodeMap.put(300, "Multiple Choices");
        httpCodeMap.put(301, "Moved Permanently");
        httpCodeMap.put(302, "Found");
        httpCodeMap.put(303, "See Other");
        httpCodeMap.put(304, "Not Modified");
        httpCodeMap.put(305, "Use Proxy");
        httpCodeMap.put(306, "Unused");
        httpCodeMap.put(307, "Temporary Redirect");
        httpCodeMap.put(308, "Permanent Redirect");
        httpCodeMap.put(400, "Bad Request");
        httpCodeMap.put(401, "Unauthorized");
        httpCodeMap.put(402, "Payment Required");
        httpCodeMap.put(403, "Forbidden");
        httpCodeMap.put(404, "Not Found");
        httpCodeMap.put(405, "Method Not Allowed");
        httpCodeMap.put(406, "Not Acceptable");
        httpCodeMap.put(407, "Proxy Authentication Required");
        httpCodeMap.put(408, "Request Timeout");
        httpCodeMap.put(409, "Conflict");
        httpCodeMap.put(410, "Gone");
        httpCodeMap.put(411, "Length Required");
        httpCodeMap.put(412, "Precondition Failed");
        httpCodeMap.put(413, "Payload Too Large");
        httpCodeMap.put(414, "URI Too Long");
        httpCodeMap.put(415, "Unsupported Media Type");
        httpCodeMap.put(416, "Range Not Satisfiable");
        httpCodeMap.put(417, "Expectation Failed");
        httpCodeMap.put(421, "Misdirected Request");
        httpCodeMap.put(422, "Unprocessable Entity");
        httpCodeMap.put(423, "Locked");
        httpCodeMap.put(424, "Failed Dependency");
        httpCodeMap.put(425, "Too Early");
        httpCodeMap.put(426, "Upgrade Required");
        httpCodeMap.put(428, "Precondition Required");
        httpCodeMap.put(429, "Too Many Requests");
        httpCodeMap.put(431, "Request Header Fields Too Large");
        httpCodeMap.put(451, "Unavailable For Legal Reasons");
        httpCodeMap.put(500, "Internal Server Error");
        httpCodeMap.put(501, "Not Implemented");
        httpCodeMap.put(502, "Bad Gateway");
        httpCodeMap.put(503, "Service Unavailable");
        httpCodeMap.put(504, "Gateway Timeout");
        httpCodeMap.put(505, "HTTP Version Not Supported");
        httpCodeMap.put(506, "Variant Also Negotiates");
        httpCodeMap.put(507, "Insufficient Storage");
        httpCodeMap.put(508, "Loop Denied");
        httpCodeMap.put(510, "Not Extended");
        httpCodeMap.put(511, "Network Authentication Required");
        HTTP_STATUS_CODE_TO_TEXT = Collections.unmodifiableMap(httpCodeMap);
    }

    FakeUrlRequest(
            Callback callback,
            Executor userExecutor,
            Executor executor,
            String url,
            boolean allowDirectExecutor,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            final boolean trafficStatsUidSet,
            final int trafficStatsUid,
            FakeCronetController fakeCronetController,
            FakeCronetEngine fakeCronetEngine,
            Collection<Object> requestAnnotations,
            String method,
            ArrayList<Map.Entry<String, String>> requestHeaders,
            UploadDataProvider uploadDataProvider,
            Executor uploadDataProviderExecutor) {
        mCurrentUrl = Objects.requireNonNull(url, "URL is required");
        mCallback = Objects.requireNonNull(callback, "Listener is required");
        mExecutor = Objects.requireNonNull(executor, "Executor is required");
        mUserExecutor =
                allowDirectExecutor ? userExecutor : new DirectPreventingExecutor(userExecutor);
        mFakeCronetController = fakeCronetController;
        mFakeCronetEngine = fakeCronetEngine;
        mAllowDirectExecutor = allowDirectExecutor;
        mRequestAnnotations = requestAnnotations;
        mHttpMethod = checkedHttpMethod(method);
        mAllHeadersList = Collections.unmodifiableList(new ArrayList<>(requestHeaders));
        mUploadDataProvider = checkedUploadDataProvider(uploadDataProvider);
        mUploadExecutor =
                uploadDataProviderExecutor == null || mAllowDirectExecutor
                        ? uploadDataProviderExecutor
                        : new DirectPreventingExecutor(uploadDataProviderExecutor);
    }

    private UploadDataProvider checkedUploadDataProvider(UploadDataProvider uploadDataProvider) {
        if (uploadDataProvider == null) {
            return null;
        }

        if (!checkHasContentTypeHeader()) {
            throw new IllegalArgumentException(
                    "Requests with upload data must have a Content-Type.");
        }
        return uploadDataProvider;
    }

    private static String checkedHttpMethod(String method) {
        if (method == null) {
            throw new NullPointerException("Method is required.");
        }
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
            throw new IllegalArgumentException("Invalid http method: " + method);
        }
    }

    @Override
    public void start() {
        synchronized (mLock) {
            if (mFakeCronetEngine.startRequest()) {
                boolean transitionedState = false;
                try {
                    transitionStates(State.NOT_STARTED, State.STARTED);
                    mAdditionalStatusDetails = Status.CONNECTING;
                    transitionedState = true;
                } finally {
                    if (!transitionedState) {
                        cleanup();
                        mFakeCronetEngine.onRequestFinished();
                    }
                }
                mUrlChain.add(mCurrentUrl);
                if (mUploadDataProvider != null) {
                    mFakeDataSink =
                            new FakeDataSink(mUploadExecutor, mExecutor, mUploadDataProvider);
                    mFakeDataSink.start(/* firstTime= */ true);
                } else {
                    fakeConnect();
                }
            } else {
                throw new IllegalStateException("This request's CronetEngine is already shutdown.");
            }
        }
    }

    /**
     * Fakes a request to a server by retrieving a response to this {@link UrlRequest} from the
     * {@link FakeCronetController}.
     */
    @GuardedBy("mLock")
    private void fakeConnect() {
        mAdditionalStatusDetails = Status.WAITING_FOR_RESPONSE;
        mCurrentFakeResponse =
                mFakeCronetController.getResponse(
                        mCurrentUrl, mHttpMethod, mAllHeadersList, mRequestBody);
        int responseCode = mCurrentFakeResponse.getHttpStatusCode();
        mUrlResponseInfo =
                new UrlResponseInfoImpl(
                        Collections.unmodifiableList(new ArrayList<>(mUrlChain)),
                        responseCode,
                        getDescriptionByCode(responseCode),
                        mCurrentFakeResponse.getAllHeadersList(),
                        mCurrentFakeResponse.getWasCached(),
                        mCurrentFakeResponse.getNegotiatedProtocol(),
                        mCurrentFakeResponse.getProxyServer(),
                        mCurrentFakeResponse.getResponseBody().length);
        mResponse = ByteBuffer.wrap(mCurrentFakeResponse.getResponseBody());
        // Check for a redirect.
        if (responseCode >= 300 && responseCode < 400) {
            processRedirectResponse();
        } else {
            closeUploadDataProvider();
            final UrlResponseInfo info = mUrlResponseInfo;
            transitionStates(State.STARTED, State.AWAITING_READ);
            executeCheckedRunnable(
                    () -> {
                        mCallback.onResponseStarted(FakeUrlRequest.this, info);
                    });
        }
    }

    /**
     * Retrieves the redirect location from the response headers and responds to the
     * {@link UrlRequest.Callback#onRedirectReceived} method. Adds the redirect URL to the chain.
     *
     * @param url the URL that the {@link FakeUrlResponse} redirected this request to
     */
    @GuardedBy("mLock")
    private void processRedirectResponse() {
        transitionStates(State.STARTED, State.REDIRECT_RECEIVED);
        if (mUrlResponseInfo.getAllHeaders().get("location") == null) {
            // Response did not have a location header, so this request must fail.
            final String prevUrl = mCurrentUrl;
            mUserExecutor.execute(
                    () -> {
                        tryToFailWithException(
                                new CronetExceptionImpl(
                                        "Request failed due to bad redirect HTTP headers",
                                        new IllegalStateException(
                                                "Response recieved from URL: "
                                                        + prevUrl
                                                        + " was a redirect, but lacked a location"
                                                        + " header.")));
                    });
            return;
        }
        String pendingRedirectUrl =
                URI.create(mCurrentUrl)
                        .resolve(mUrlResponseInfo.getAllHeaders().get("location").get(0))
                        .toString();
        mCurrentUrl = pendingRedirectUrl;
        mUrlChain.add(mCurrentUrl);
        transitionStates(State.REDIRECT_RECEIVED, State.AWAITING_FOLLOW_REDIRECT);
        final UrlResponseInfo info = mUrlResponseInfo;
        mExecutor.execute(
                () -> {
                    executeCheckedRunnable(
                            () -> {
                                mCallback.onRedirectReceived(
                                        FakeUrlRequest.this, info, pendingRedirectUrl);
                            });
                });
    }

    @Override
    public void read(ByteBuffer buffer) {
        // Entering {@link #State.READING} is somewhat redundant because the entire response is
        // already acquired. We should still transition so that the fake {@link UrlRequest} follows
        // the same state flow as a real request.
        Preconditions.checkHasRemaining(buffer);
        Preconditions.checkDirect(buffer);
        synchronized (mLock) {
            transitionStates(State.AWAITING_READ, State.READING);
            final UrlResponseInfo info = mUrlResponseInfo;
            if (mResponse.hasRemaining()) {
                transitionStates(State.READING, State.AWAITING_READ);
                fillBufferWithResponse(buffer);
                mExecutor.execute(
                        () -> {
                            executeCheckedRunnable(
                                    () -> {
                                        mCallback.onReadCompleted(
                                                FakeUrlRequest.this, info, buffer);
                                    });
                        });
            } else {
                final RefCountDelegate inflightDoneCallbackCount = setTerminalState(State.COMPLETE);
                if (inflightDoneCallbackCount != null) {
                    mUserExecutor.execute(
                            () -> {
                                mCallback.onSucceeded(FakeUrlRequest.this, info);
                                inflightDoneCallbackCount.decrement();
                            });
                }
            }
        }
    }

    /**
     * Puts as much of the remaining response as will fit into the {@link ByteBuffer} and removes
     * that part of the string from the response left to send.
     *
     * @param buffer the {@link ByteBuffer} to put the response into
     */
    @GuardedBy("mLock")
    private void fillBufferWithResponse(ByteBuffer buffer) {
        final int maxTransfer = Math.min(buffer.remaining(), mResponse.remaining());
        ByteBuffer temp = mResponse.duplicate();
        temp.limit(temp.position() + maxTransfer);
        buffer.put(temp);
        mResponse.position(mResponse.position() + maxTransfer);
    }

    @Override
    public void followRedirect() {
        synchronized (mLock) {
            transitionStates(State.AWAITING_FOLLOW_REDIRECT, State.STARTED);
            if (mFakeDataSink != null) {
                mFakeDataSink = new FakeDataSink(mUploadExecutor, mExecutor, mUploadDataProvider);
                mFakeDataSink.start(/* firstTime= */ false);
            } else {
                fakeConnect();
            }
        }
    }

    @Override
    public void cancel() {
        synchronized (mLock) {
            if (mState == State.NOT_STARTED || isDone()) {
                return;
            }

            final UrlResponseInfo info = mUrlResponseInfo;
            final RefCountDelegate inflightDoneCallbackCount = setTerminalState(State.CANCELLED);
            if (inflightDoneCallbackCount != null) {
                mUserExecutor.execute(
                        () -> {
                            mCallback.onCanceled(FakeUrlRequest.this, info);
                            inflightDoneCallbackCount.decrement();
                        });
            }
        }
    }

    @Override
    public void getStatus(final StatusListener listener) {
        synchronized (mLock) {
            int extraStatus = mAdditionalStatusDetails;

            @UrlRequestUtil.StatusValues final int status;
            switch (mState) {
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
                    throw new IllegalStateException("Switch is exhaustive: " + mState);
            }
            mUserExecutor.execute(
                    new Runnable() {
                        @Override
                        public void run() {
                            listener.onStatus(status);
                        }
                    });
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mLock) {
            return mState == State.COMPLETE || mState == State.ERROR || mState == State.CANCELLED;
        }
    }

    /**
     * Swaps from the expected state to a new state. If the swap fails, and it's not
     * due to an earlier error or cancellation, throws an exception.
     */
    @GuardedBy("mLock")
    private void transitionStates(@State int expected, @State int newState) {
        if (mState == expected) {
            mState = newState;
        } else {
            if (!(mState == State.CANCELLED || mState == State.ERROR)) {
                // TODO(crbug.com/40915368): Use Enums for state instead for better error messages.
                throw new IllegalStateException(
                        "Invalid state transition - expected " + expected + " but was " + mState);
            }
        }
    }

    /**
     * Calls the callback's onFailed method if this request is not complete. Should be executed on
     * the {@code mUserExecutor}, unless the error is a {@link InlineExecutionProhibitedException}
     * produced by the {@code mUserExecutor}.
     *
     * @param e the {@link CronetException} that the request should pass to the callback.
     *
     */
    private void tryToFailWithException(CronetException e) {
        synchronized (mLock) {
            mCronetException = e;
            final RefCountDelegate inflightDoneCallbackCount = setTerminalState(State.ERROR);
            if (inflightDoneCallbackCount != null) {
                mCallback.onFailed(FakeUrlRequest.this, mUrlResponseInfo, e);
                inflightDoneCallbackCount.decrement();
            }
        }
    }

    /**
     * Execute a {@link CheckedRunnable} and call the {@link UrlRequest.Callback#onFailed} method
     * if there is an exception and we can change to {@link State.ERROR}. Used to communicate with
     * the {@link UrlRequest.Callback} methods using the executor provided by the constructor. This
     * should be the last call in the critical section. If this is not the last call in a critical
     * section, we risk modifying shared resources in a recursive call to another method
     * guarded by the {@code mLock}. This is because in Java synchronized blocks are reentrant.
     *
     * @param checkedRunnable the runnable to execute
     */
    private void executeCheckedRunnable(JavaUrlRequestUtils.CheckedRunnable checkedRunnable) {
        try {
            mUserExecutor.execute(
                    () -> {
                        try {
                            checkedRunnable.run();
                        } catch (Exception e) {
                            tryToFailWithException(
                                    new CallbackExceptionImpl(
                                            "Exception received from UrlRequest.Callback", e));
                        }
                    });
        } catch (InlineExecutionProhibitedException e) {
            // Don't try to fail using the {@code mUserExecutor} because it produced this error.
            tryToFailWithException(
                    new CronetExceptionImpl("Exception posting task to executor", e));
        }
    }

    /**
     * Check the current state and if the request is started, but not complete, failed, or
     * cancelled, change to the terminal state and call {@link FakeCronetEngine#onDestroyed}. This
     * method ensures {@link FakeCronetEngine#onDestroyed} is only called once.
     *
     * @param terminalState the terminal state to set; one of {@link State.ERROR},
     * {@link State.COMPLETE}, or {@link State.CANCELLED}
     * @return a refcount to decrement after the terminal callback is called, or
     * null if the terminal state wasn't set.
     */
    @GuardedBy("mLock")
    private RefCountDelegate setTerminalState(@State int terminalState) {
        switch (mState) {
            case State.NOT_STARTED:
                throw new IllegalStateException("Can't enter terminal state before start");
            case State.ERROR: // fallthrough
            case State.COMPLETE: // fallthrough
            case State.CANCELLED:
                return null; // Already in a terminal state
            default:
                {
                    mState = terminalState;
                    final RefCountDelegate inflightDoneCallbackCount =
                            new RefCountDelegate(mFakeCronetEngine::onRequestFinished);
                    reportRequestFinished(inflightDoneCallbackCount);
                    cleanup();
                    return inflightDoneCallbackCount;
                }
        }
    }

    private void reportRequestFinished(RefCountDelegate inflightDoneCallbackCount) {
        synchronized (mLock) {
            mFakeCronetEngine.reportRequestFinished(
                    new FakeRequestFinishedInfo(
                            mCurrentUrl,
                            mRequestAnnotations,
                            getRequestFinishedReason(),
                            mUrlResponseInfo,
                            mCronetException),
                    inflightDoneCallbackCount);
        }
    }

    @RequestFinishedInfoImpl.FinishedReason
    @GuardedBy("mLock")
    private int getRequestFinishedReason() {
        synchronized (mLock) {
            switch (mState) {
                case State.COMPLETE:
                    return RequestFinishedInfo.SUCCEEDED;
                case State.ERROR:
                    return RequestFinishedInfo.FAILED;
                case State.CANCELLED:
                    return RequestFinishedInfo.CANCELED;
                default:
                    throw new IllegalStateException(
                            "Request should be in terminal state before calling"
                                + " getRequestFinishedReason");
            }
        }
    }

    @GuardedBy("mLock")
    private void cleanup() {
        closeUploadDataProvider();
        mFakeCronetEngine.onRequestDestroyed();
    }

    /**
     * Executed only once after the request has finished using the {@link UploadDataProvider}.
     * Closes the {@link UploadDataProvider} if it exists and has not already been closed.
     */
    @GuardedBy("mLock")
    private void closeUploadDataProvider() {
        if (mUploadDataProvider != null && !mUploadProviderClosed) {
            try {
                mUploadExecutor.execute(
                        uploadErrorSetting(
                                () -> {
                                    synchronized (mLock) {
                                        mUploadDataProvider.close();
                                        mUploadProviderClosed = true;
                                    }
                                }));
            } catch (RejectedExecutionException e) {
                Log.e(TAG, "Exception when closing uploadDataProvider", e);
            }
        }
    }

    /**
     * Wraps a {@link CheckedRunnable} in a runnable that will attempt to fail the request if there
     * is an exception.
     *
     * @param delegate the {@link CheckedRunnable} to try to run
     * @return a {@link Runnable} that wraps the delegate runnable.
     */
    private Runnable uploadErrorSetting(final CheckedRunnable delegate) {
        return new Runnable() {
            @Override
            public void run() {
                try {
                    delegate.run();
                } catch (Throwable t) {
                    enterUploadErrorState(t);
                }
            }
        };
    }

    /**
     * Fails the request with an error. Called when uploading the request body using an
     * {@link UploadDataProvider} fails.
     *
     * @param error the error that caused this request to fail which should be returned to the
     *              {@link UrlRequest.Callback}
     */
    private void enterUploadErrorState(final Throwable error) {
        synchronized (mLock) {
            executeCheckedRunnable(
                    () ->
                            tryToFailWithException(
                                    new CronetExceptionImpl(
                                            "Exception received from UploadDataProvider", error)));
        }
    }

    /**
     * Adapted from {@link JavaUrlRequest.OutputStreamDataSink}. Stores the received message in a
     * {@link ByteArrayOutputStream} and transfers it to the {@code mRequestBody} when the response
     * has been fully acquired.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    final class FakeDataSink extends JavaUploadDataSinkBase {
        private final ByteArrayOutputStream mBodyStream = new ByteArrayOutputStream();
        private final WritableByteChannel mBodyChannel = Channels.newChannel(mBodyStream);

        FakeDataSink(final Executor userExecutor, Executor executor, UploadDataProvider provider) {
            super(userExecutor, executor, provider);
        }

        @Override
        public Runnable getErrorSettingRunnable(JavaUrlRequestUtils.CheckedRunnable runnable) {
            return new Runnable() {
                @Override
                public void run() {
                    try {
                        runnable.run();
                    } catch (Throwable t) {
                        mUserExecutor.execute(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        tryToFailWithException(
                                                new CronetExceptionImpl("System error", t));
                                    }
                                });
                    }
                }
            };
        }

        @Override
        protected Runnable getUploadErrorSettingRunnable(
                JavaUrlRequestUtils.CheckedRunnable runnable) {
            return uploadErrorSetting(runnable);
        }

        @Override
        protected void processUploadError(final Throwable error) {
            enterUploadErrorState(error);
        }

        @Override
        protected int processSuccessfulRead(ByteBuffer buffer) throws IOException {
            return mBodyChannel.write(buffer);
        }

        /**
         * Terminates the upload stage of the request. Writes the received bytes to the byte array:
         * {@code mRequestBody}. Connects to the current URL for this request.
         */
        @Override
        protected void finish() throws IOException {
            synchronized (mLock) {
                mRequestBody = mBodyStream.toByteArray();
                fakeConnect();
            }
        }

        @Override
        protected void initializeRead() throws IOException {
            // Nothing to do before every read in this implementation.
        }

        @Override
        protected void initializeStart(long totalBytes) {
            // Nothing to do to initialize the upload in this implementation.
        }
    }

    /**
     * Verifies that the "content-type" header is present. Must be checked before an {@link
     * UploadDataProvider} is premitted to be set.
     *
     * @return true if the "content-type" header is present in the request headers.
     */
    private boolean checkHasContentTypeHeader() {
        for (Map.Entry<String, String> entry : mAllHeadersList) {
            if (entry.getKey().equalsIgnoreCase("content-type")) {
                return true;
            }
        }
        return false;
    }

    /**
     * Gets a human readable description for a HTTP status code.
     *
     * @param code the code to retrieve the status for
     * @return the HTTP status text as a string
     */
    private static String getDescriptionByCode(Integer code) {
        return HTTP_STATUS_CODE_TO_TEXT.containsKey(code)
                ? HTTP_STATUS_CODE_TO_TEXT.get(code)
                : "Unassigned";
    }
}
