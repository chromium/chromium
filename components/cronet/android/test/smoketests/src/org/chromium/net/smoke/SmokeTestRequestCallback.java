// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import android.os.ConditionVariable;

import org.chromium.net.CronetException;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** A simple boilerplate implementation of {@link UrlRequest.Callback} that is used by smoke tests. */
class SmokeTestRequestCallback extends UrlRequest.Callback {
    private static final int READ_BUFFER_SIZE = 10000;

    // An executor that is used to execute {@link UrlRequest.Callback UrlRequest callbacks}.
    private ExecutorService mExecutor = Executors.newSingleThreadExecutor();

    // Signals when the request is done either successfully or not.
    private final ConditionVariable mDone = new ConditionVariable();

    // The state of the request.
    public enum State {
        NotSet,
        Succeeded,
        Failed,
        Canceled
    }

    // The current state of the request.
    private State mState = State.NotSet;

    // Response info of the finished request.
    private UrlResponseInfo mResponseInfo;

    // Holds an error if the request failed.
    private CronetException mError;

    @Override
    public void onRedirectReceived(UrlRequest request, UrlResponseInfo info, String newLocationUrl)
            throws Exception {
        request.followRedirect();
    }

    @Override
    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) throws Exception {
        request.read(ByteBuffer.allocateDirect(READ_BUFFER_SIZE));
    }

    @Override
    public void onReadCompleted(UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer)
            throws Exception {
        request.read(ByteBuffer.allocateDirect(READ_BUFFER_SIZE));
    }

    @Override
    public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
        done(State.Succeeded, info);
    }

    @Override
    public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
        mError = error;
        done(State.Failed, info);
    }

    @Override
    public void onCanceled(UrlRequest request, UrlResponseInfo info) {
        done(State.Canceled, info);
    }

    /**
     * Returns the request executor.
     *
     * @return the executor.
     */
    public Executor getExecutor() {
        return mExecutor;
    }

    /** Blocks until the request is either succeeded, failed or canceled. */
    public void blockForDone() {
        mDone.block();
    }

    /**
     * Returns the final state of the request.
     *
     * @return the state.
     */
    public State getFinalState() {
        return mState;
    }

    /**
     * Returns an error that was passed to {@link #onFailed}  when the request failed.
     *
     * @return the error if the request failed; {@code null} otherwise.
     */
    public CronetException getFailureError() {
        return mError;
    }

    /**
     * Returns {@link UrlResponseInfo} of the finished response.
     *
     * @return the response info. {@code null} if the request hasn't completed yet.
     */
    public UrlResponseInfo getResponseInfo() {
        return mResponseInfo;
    }

    private void done(State finalState, UrlResponseInfo responseInfo) {
        assertThat(mState).isEqualTo(State.NotSet);
        mResponseInfo = responseInfo;
        mState = finalState;
        mDone.open();
    }
}
