// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.MainThread;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.net.NetworkChangeNotifier;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * A helper class to encapsulate network connection retry logic for AuthTasks.
 *
 * <p>The task will be run on the background thread. If it encounters a transient error, it will
 * wait for a network change and retry up to {@link #MAX_TRIES} times.
 */
@NullMarked
public class ConnectionRetry implements NetworkChangeNotifier.ConnectionTypeObserver {
    /**
     * Authentication Task used together with ConnectionRetry class. If a transient error occurs the
     * task may be tried again {@link #MAX_TRIES} number of times.
     */
    public interface AuthTask {
        /** Runs the AuthTask. */
        @Nullable AccessTokenData run() throws AuthException;

        /** Called with the result when the AuthTask succeeded. */
        void onSuccess(@Nullable AccessTokenData result);

        /** Called with the {@link GoogleServiceAuthError} when the AuthTask failed. */
        void onFailure(GoogleServiceAuthError authError);
    }

    private static final String TAG = "AuthTaskRetry";
    private static final int MAX_TRIES = 3;

    private final AuthTask mAuthTask;
    private final AtomicInteger mNumTries;

    /** Run the given {@link AuthTask} with {@link #MAX_TRIES} times. */
    @MainThread
    public static void runAuthTask(AuthTask authTask) {
        ThreadUtils.assertOnUiThread();
        new ConnectionRetry(authTask).attempt();
    }

    private ConnectionRetry(AuthTask authTask) {
        mAuthTask = authTask;
        mNumTries = new AtomicInteger(0);
    }

    /**
     * Tries running the {@link AuthTask} in the background. This object is never registered as a
     * {@link NetworkChangeNotifier.ConnectionTypeObserver} when this method is called.
     */
    private void attempt() {
        ThreadUtils.assertOnUiThread();
        // Clear any transient error.
        new AsyncTask<@Nullable AccessTokenData>() {
            private GoogleServiceAuthError mAuthError =
                    new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE);

            @Override
            public @Nullable AccessTokenData doInBackground() {
                try {
                    return mAuthTask.run();
                } catch (AuthException ex) {
                    Log.w(TAG, "Failed to perform auth task: %s", ex.stringifyCausalChain());
                    mAuthError = ex.getAuthError();
                    return null;
                }
            }

            @Override
            public void onPostExecute(@Nullable AccessTokenData result) {
                if (mAuthError.getState() == GoogleServiceAuthErrorState.NONE) {
                    mAuthTask.onSuccess(result);
                    return;
                }

                if (!mAuthError.isTransientError()
                        || mNumTries.incrementAndGet() >= MAX_TRIES
                        || !NetworkChangeNotifier.isInitialized()) {
                    // Permanent error, ran out of tries, or we can't listen for network
                    // change events; give up.
                    mAuthTask.onFailure(mAuthError);
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
