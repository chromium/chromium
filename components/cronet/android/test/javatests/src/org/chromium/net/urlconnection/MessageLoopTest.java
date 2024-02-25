// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.io.InterruptedIOException;
import java.net.SocketTimeoutException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;

/** Tests the MessageLoop implementation. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class MessageLoopTest {
    private Thread mTestThread;
    private final ExecutorService mExecutorService =
            Executors.newSingleThreadExecutor(new ExecutorThreadFactory());

    private class ExecutorThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(Runnable r) {
            mTestThread = new Thread(r);
            return mTestThread;
        }
    }

    private boolean mFailed;

    @Test
    @SmallTest
    public void testInterrupt() throws Exception {
        final MessageLoop loop = new MessageLoop();
        assertThat(loop.isRunning()).isFalse();
        Future future =
                mExecutorService.submit(
                        new Runnable() {
                            @Override
                            public void run() {
                                assertThrows(InterruptedIOException.class, loop::loop);
                            }
                        });
        Thread.sleep(1000);
        assertThat(loop.isRunning()).isTrue();
        assertThat(loop.hasLoopFailed()).isFalse();
        mTestThread.interrupt();
        future.get();
        assertThat(loop.isRunning()).isFalse();
        assertThat(loop.hasLoopFailed()).isTrue();
        // Re-spinning the message loop is not allowed after interrupt.
        mExecutorService
                .submit(
                        new Runnable() {
                            @Override
                            public void run() {
                                assertThrows(InterruptedIOException.class, loop::loop);
                            }
                        })
                .get();
    }

    @Test
    @SmallTest
    public void testTaskFailed() throws Exception {
        final MessageLoop loop = new MessageLoop();
        assertThat(loop.isRunning()).isFalse();
        Future future =
                mExecutorService.submit(
                        new Runnable() {
                            @Override
                            public void run() {
                                assertThrows(NullPointerException.class, loop::loop);
                            }
                        });
        Runnable failedTask =
                new Runnable() {
                    @Override
                    public void run() {
                        throw new NullPointerException();
                    }
                };
        Thread.sleep(1000);
        assertThat(loop.isRunning()).isTrue();
        assertThat(loop.hasLoopFailed()).isFalse();
        loop.execute(failedTask);
        future.get();
        assertThat(loop.isRunning()).isFalse();
        assertThat(loop.hasLoopFailed()).isTrue();
        // Re-spinning the message loop is not allowed after exception.
        mExecutorService
                .submit(
                        new Runnable() {
                            @Override
                            public void run() {
                                assertThrows(NullPointerException.class, loop::loop);
                            }
                        })
                .get();
    }

    @Test
    @SmallTest
    public void testLoopWithTimeout() throws Exception {
        final MessageLoop loop = new MessageLoop();
        assertThat(loop.isRunning()).isFalse();
        // The MessageLoop queue is empty. Use a timeout of 100ms to check that
        // it doesn't block forever.
        assertThrows(SocketTimeoutException.class, () -> loop.loop(100));
        assertThat(loop.isRunning()).isFalse();
    }
}
