// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.fcm;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.WorkerThread;

import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.installations.FirebaseInstallations;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Singleton class that manages Firebase SDK initialization and Firebase Installation ID operations.
 *
 * <p>Lazy initialization in {@link #getInstance()} must be performed on a background worker thread
 * to avoid blocking the UI thread with disk I/O during FirebaseApp and FirebaseInstallations
 * initialization.
 */
@NullMarked
public class FcmManager {
    private static final String TAG = "FcmManager";
    private static final String APP_NAME = "ChromeFcmApp";

    // TODO(crbug.com/556011365): move the following constants to C++ code and provide real values.
    private static final String DEFAULT_PROJECT_ID = "unused-project-id";
    private static final String DEFAULT_APP_ID = "1:123456789012:android:0123456789abcdef012345";

    // Synchronizes lazy initialization of sInstance across worker threads.
    private static final Object sLock = new Object();
    private static @Nullable FcmManager sInstance;

    private final FirebaseApp mFirebaseApp;
    private final FirebaseInstallations mInstallations;

    /**
     * Returns the singleton FcmManager instance, initializing it lazily if needed.
     *
     * <p>Must only be called on a background worker thread to prevent blocking the UI thread during
     * initialization.
     */
    @WorkerThread
    public static FcmManager getInstance() {
        ThreadUtils.assertOnBackgroundThread();
        synchronized (sLock) {
            if (sInstance == null) {
                sInstance =
                        new FcmManager(ContextUtils.getApplicationContext(), DEFAULT_PROJECT_ID);
            }
            return sInstance;
        }
    }

    /** Returns whether FcmManager has been initialized. */
    public static boolean isInitialized() {
        synchronized (sLock) {
            return sInstance != null;
        }
    }

    /** Sets a test instance. */
    public static void setInstanceForTesting(@Nullable FcmManager testInstance) {
        synchronized (sLock) {
            var previous = sInstance;
            sInstance = testInstance;
            ResettersForTesting.register(
                    () -> {
                        synchronized (sLock) {
                            sInstance = previous;
                        }
                    });
        }
    }

    protected FcmManager(Context context, String projectId) {
        assert !TextUtils.isEmpty(projectId) : "Project ID must not be empty.";
        FirebaseOptions options =
                new FirebaseOptions.Builder()
                        .setProjectId(projectId)
                        .setApplicationId(DEFAULT_APP_ID)
                        .setApiKey("unused")
                        .build();
        FirebaseApp app = null;
        for (FirebaseApp existingApp : FirebaseApp.getApps(context)) {
            if (APP_NAME.equals(existingApp.getName())) {
                app = existingApp;
                break;
            }
        }
        if (app == null) {
            app = FirebaseApp.initializeApp(context, options, APP_NAME);
        }
        mFirebaseApp = app;
        mInstallations = FirebaseInstallations.getInstance(mFirebaseApp);
    }

    protected FcmManager(FirebaseApp app, FirebaseInstallations installations) {
        mFirebaseApp = app;
        mInstallations = installations;
    }

    /**
     * Fetches the Firebase Installation ID (FID) asynchronously.
     *
     * @param callback Callback invoked with the Installation ID, or empty string on failure.
     */
    public void fetchInstallationId(Callback<String> callback) {
        mInstallations
                .getId()
                .addOnCompleteListener(
                        task -> {
                            if (task.isSuccessful()) {
                                callback.onResult(task.getResult());
                            } else {
                                Exception exception = task.getException();
                                if (exception != null) {
                                    Log.e(TAG, "Failed to fetch installation ID", exception);
                                } else {
                                    Log.e(TAG, "Failed to fetch installation ID");
                                }
                                callback.onResult("");
                            }
                        });
    }

    /**
     * Deletes the Firebase Installation ID asynchronously.
     *
     * @param callback Callback invoked with true if deletion succeeded, false otherwise.
     */
    public void deleteInstallationId(Callback<Boolean> callback) {
        mInstallations
                .delete()
                .addOnCompleteListener(
                        task -> {
                            if (!task.isSuccessful()) {
                                Exception exception = task.getException();
                                if (exception != null) {
                                    Log.e(TAG, "Failed to delete installation ID", exception);
                                } else {
                                    Log.e(TAG, "Failed to delete installation ID");
                                }
                            }
                            callback.onResult(task.isSuccessful());
                        });
    }

    /** Returns the underlying FirebaseApp. */
    public FirebaseApp getFirebaseApp() {
        return mFirebaseApp;
    }
}
