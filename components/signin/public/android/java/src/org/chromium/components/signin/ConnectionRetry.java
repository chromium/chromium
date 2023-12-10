// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.NetworkChangeNotifier;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * A helper class to encapsulate network connection retry logic for AuthTasks.
 *
 * The task will be run on the background thread. If it encounters a transient error, it
 * will wait for a network change and retry up to {@link #MAX_TRIES} times.
 *
 * @param <T> Return type of the AuthTask launched by ConnectionRetry.
 */
public class ConnectionRetry<T> implements NetworkChangeNotifier.ConnectionTypeObserver {
    /**
     * Authentication Task used together with ConnectionRetry class.
     * @param <T> Return type of the AuthTask.
     */
    public interface AuthTask<T> {
        /** Runs the AuthTask. */
        T run() throws AuthException;

        /** Called with the result when the AuthTask succeeded. */
        default void onSuccess(T result) {}

        /** Called with the result when the AuthTask failed. */
        default void onFailure(boolean isTransientError) {}
    }

    private static final String TAG = "AuthTaskRetry";
    private static final int MAX_TRIES = 3;

    private final AuthTask<T> mAuthTask;
    private final AtomicInteger mNumTries;
    private final AtomicBoolean mIsTransientError;

    /** Run the given {@link AuthTask} with {@link #MAX_TRIES} times. */
    public static <T> void runAuthTask(AuthTask<T> authTask) {
        new ConnectionRetry<>(authTask).attempt();
    }

    private ConnectionRetry(AuthTask<T> authTask) {
        mAuthTask = authTask;
        mNumTries = new AtomicInteger(0);
        mIsTransientError = new AtomicBoolean(false);
    }

    /**
     * Tries running the {@link AuthTask} in the background. This object is never registered
     * as a {@link NetworkChangeNotifier.ConnectionTypeObserver} when this method is called.
     */
    private void attempt() {
        ThreadUtils.assertOnUiThread();
        // Clear any transient error.
        mIsTransientError.set(false);
        new AsyncTask<T>() {
            @Override
            public T doInBackground() {
                try {
                    return mAuthTask.run();
                } catch (AuthException ex) {
                    Log.w(TAG, "Failed to perform auth task: %s", ex.stringifyCausalChain());
                    mIsTransientError.set(ex.isTransientError());
                }
                return null;
            }

            @Override
            public void onPostExecute(T result) {
                if (result != null) {
                    mAuthTask.onSuccess(result);
                } else if (!mIsTransientError.get()
                        || mNumTries.incrementAndGet() >= MAX_TRIES
                        || !NetworkChangeNotifier.isInitialized()) {
                    // Permanent error, ran out of tries, or we can't listen for network
                    // change events; give up.
                    mAuthTask.onFailure(mIsTransientError.get());
                } else {
                    // Transient error with tries left; register for another attempt.
                    NetworkChangeNotifier.addConnectionTypeObserver(ConnectionRetry.this);
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        assert mNumTries.get() < MAX_TRIES;
        if (NetworkChangeNotifier.isOnline()) {
            // The network is back; stop listening and try again.
            NetworkChangeNotifier.removeConnectionTypeObserver(this);
            attempt();
        }
    }
}
