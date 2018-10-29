// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.ArrayDeque;

/**
 * Schedules tasks to run one after another.
 *
 * If nothing is running, scheduled tasks execute synchronously. Any tasks posted by a scheduled
 * task are deferred until the current task finishes. This grants a limited notion of single-
 * threaded atomicity. Note that this is NOT thread-safe!
 */
public class Sequencer {
    private static final int MAX_RECURSION_DEPTH = 65536;
    private boolean mIsRunning;
    private final ArrayDeque<Runnable> mMessageQueue = new ArrayDeque<>();

    /**
     * Runs the task synchronously, or, if a sequence()d task is already running, posts the task
     * to a queue, whose items will be run synchronously when the current task is finished.
     */
    public void sequence(Runnable task) {
        mMessageQueue.add(task);
        if (mIsRunning) return;
        for (int count = 0; !mMessageQueue.isEmpty(); count++) {
            if (count == MAX_RECURSION_DEPTH) {
                throw new InceptionException(
                        "Too many nested sequenced tasks posted from one sequenced call! "
                        + "Is there an infinite loop?");
            }
            sequenceInternal(mMessageQueue.removeFirst());
        }
    }

    private void sequenceInternal(Runnable task) {
        mIsRunning = true;
        task.run();
        mIsRunning = false;
    }

    public boolean inSequence() {
        return mIsRunning;
    }

    /**
     * Thrown if a task in a task in a task... seems to go on forever. Of course we can't detect
     * whether it actually goes on forever because of the halting problem, so we give up after a
     * threshold is exceeded.
     */
    public static class InceptionException extends RuntimeException {
        public InceptionException(String message) {
            super(message);
        }
    }
}
