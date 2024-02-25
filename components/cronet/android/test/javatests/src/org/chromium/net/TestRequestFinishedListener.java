// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.os.ConditionVariable;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/** RequestFinishedInfo.Listener for testing, which saves the RequestFinishedInfo */
public class TestRequestFinishedListener extends RequestFinishedInfo.Listener {
    private final ConditionVariable mBlock;
    private final ConditionVariable mBlockListener = new ConditionVariable(true);
    private RequestFinishedInfo mRequestInfo;
    private boolean mMakeListenerThrow;

    // TODO(mgersh): it's weird that you can use either this constructor or blockUntilDone() but
    // not both. Either clean it up or document why it has to work this way.
    public TestRequestFinishedListener(Executor executor) {
        super(executor);
        mBlock = new ConditionVariable();
    }

    public TestRequestFinishedListener() {
        super(Executors.newSingleThreadExecutor());
        mBlock = new ConditionVariable();
    }

    public void makeListenerThrow() {
        mMakeListenerThrow = true;
    }

    public RequestFinishedInfo getRequestInfo() {
        return mRequestInfo;
    }

    public void blockListener() {
        mBlockListener.close();
    }

    public void unblockListener() {
        mBlockListener.open();
    }

    @Override
    public void onRequestFinished(RequestFinishedInfo requestInfo) {
        assertWithMessage("onRequestFinished called repeatedly").that(mRequestInfo).isNull();
        assertThat(requestInfo).isNotNull();
        mRequestInfo = requestInfo;
        mBlock.open();
        mBlockListener.block();
        if (mMakeListenerThrow) {
            throw new IllegalStateException(
                    "TestRequestFinishedListener throwing exception as requested");
        }
    }

    public void blockUntilDone() {
        mBlock.block();
    }

    public void reset() {
        mBlock.close();
        mRequestInfo = null;
    }
}
