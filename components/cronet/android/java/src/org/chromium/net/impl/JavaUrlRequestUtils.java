// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.IntDef;

import org.chromium.net.InlineExecutionProhibitedException;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.Executor;

/**
 * Utilities for Java-based UrlRequest implementations.
 * {@hide}
 */
public final class JavaUrlRequestUtils {
    /**
     * State interface for keeping track of the internal state of a {@link UrlRequest}.
     *
     *               /- AWAITING_FOLLOW_REDIRECT <- REDIRECT_RECEIVED <-\     /- READING <--\
     *               |                                                  |     |             |
     *               V                                                  /     V             /
     * NOT_STARTED -> STARTED -----------------------------------------------> AWAITING_READ -------
     * --> COMPLETE
     *
     *
     */
    @IntDef({
        State.NOT_STARTED,
        State.STARTED,
        State.REDIRECT_RECEIVED,
        State.AWAITING_FOLLOW_REDIRECT,
        State.AWAITING_READ,
        State.READING,
        State.ERROR,
        State.COMPLETE,
        State.CANCELLED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int NOT_STARTED = 0;
        int STARTED = 1;
        int REDIRECT_RECEIVED = 2;
        int AWAITING_FOLLOW_REDIRECT = 3;
        int AWAITING_READ = 4;
        int READING = 5;
        int ERROR = 6;
        int COMPLETE = 7;
        int CANCELLED = 8;
    }

    /**
     *  Interface used to run commands that could throw an exception. Specifically useful for
     *  calling {@link UrlRequest.Callback}s on a user-supplied executor.
     */
    public interface CheckedRunnable {
        void run() throws Exception;
    }

    /** Executor that detects and throws if its mDelegate runs a submitted runnable inline. */
    public static final class DirectPreventingExecutor implements Executor {
        private final Executor mDelegate;

        /**
         * Constructs an {@link DirectPreventingExecutor} that executes {@link runnable}s on the
         * provided {@link Executor}.
         *
         * @param delegate the {@link Executor} used to run {@link Runnable}s
         */
        public DirectPreventingExecutor(Executor delegate) {
            this.mDelegate = delegate;
        }

        /**
         * Executes a {@link Runnable} on this {@link Executor} and throws an exception if it is
         * being run on the same thread as the calling thread.
         *
         * @param command the {@link Runnable} to attempt to run
         */
        @Override
        public void execute(Runnable command) {
            Thread currentThread = Thread.currentThread();
            InlineCheckingRunnable runnable = new InlineCheckingRunnable(command, currentThread);
            mDelegate.execute(runnable);
            // This next read doesn't require synchronization; only the current thread could have
            // written to runnable.mExecutedInline.
            if (runnable.mExecutedInline != null) {
                throw runnable.mExecutedInline;
            } else {
                // It's possible that this method is being called on an executor, and the runnable
                // that was just queued will run on this thread after the current runnable returns.
                // By nulling out the mCallingThread field, the InlineCheckingRunnable's current
                // thread comparison will not fire.
                //
                // Java reference assignment is always atomic (no tearing, even on 64-bit VMs, see
                // JLS 17.7), but other threads aren't guaranteed to ever see updates without
                // something like locking, volatile, or AtomicReferences. We're ok in
                // this instance, since this write only needs to be seen in the case that
                // InlineCheckingRunnable.run() runs on the same thread as this execute() method.
                runnable.mCallingThread = null;
            }
        }

        private static final class InlineCheckingRunnable implements Runnable {
            private final Runnable mCommand;
            private Thread mCallingThread;
            private InlineExecutionProhibitedException mExecutedInline;

            private InlineCheckingRunnable(Runnable command, Thread callingThread) {
                this.mCommand = command;
                this.mCallingThread = callingThread;
            }

            @Override
            public void run() {
                if (Thread.currentThread() == mCallingThread) {
                    // Can't throw directly from here, since the delegate executor could catch this
                    // exception.
                    mExecutedInline = new InlineExecutionProhibitedException();
                    return;
                }
                mCommand.run();
            }
        }
    }
}
