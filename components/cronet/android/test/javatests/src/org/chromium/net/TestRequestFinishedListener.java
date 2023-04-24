// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.os.ConditionVariable;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * RequestFinishedInfo.Listener for testing, which saves the RequestFinishedInfo
 */
public class TestRequestFinishedListener extends RequestFinishedInfo.Listener {
    private final ConditionVariable mBlock;
    private RequestFinishedInfo mRequestInfo;

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

    public RequestFinishedInfo getRequestInfo() {
        return mRequestInfo;
    }

    @Override
    public void onRequestFinished(RequestFinishedInfo requestInfo) {
        assertNull("onRequestFinished called repeatedly", mRequestInfo);
        assertNotNull(requestInfo);
        mRequestInfo = requestInfo;
        mBlock.open();
    }

    public void blockUntilDone() {
        mBlock.block();
    }

    public void reset() {
        mBlock.close();
        mRequestInfo = null;
    }
}
