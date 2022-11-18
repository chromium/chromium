// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.chromecast.base.Scope;

import java.util.concurrent.Executor;
import java.util.function.Consumer;

/**
 * Runs a task on a worker thread, then run the callback with the result on the UI thread.
 *
 * This is a slightly less verbose way of doing asynchronous work than using
 * org.chromium.base.task.AsyncTask directly.
 */
public class AsyncTaskRunner {
    private final Executor mExecutor;

    public AsyncTaskRunner(Executor executor) {
        mExecutor = executor;
    }

    /**
     * Schedules work on this runner's executor, with the result provided to the given callback.
     *
     * The returned Scope will cancel the scheduled task if close()d.
     *
     * Since this returns a Scope, a function that returns the result of doAsync() can easily be
     * used as an Observer in an Observable#subscribe() call, which can be used to cancel running
     * tasks if the Observable deactivates.
     */
    public <T> Scope doAsync(Supplier<T> task, Consumer<? super T> callback) {
        AsyncTask<T> asyncTask = new AsyncTask<T>() {
            @Override
            protected T doInBackground() {
                return task.get();
            }
            @Override
            protected void onPostExecute(T result) {
                callback.accept(result);
            }
        };
        asyncTask.executeOnExecutor(mExecutor);
        return () -> asyncTask.cancel(false);
    }
}
