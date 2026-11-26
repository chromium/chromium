// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.gcm_driver.InstanceIDFlags;
import org.chromium.components.gcm_driver.LazySubscriptionsManager;
import org.chromium.components.gcm_driver.SubscriptionFlagManager;

import java.io.IOException;

/**
 * Wraps InstanceIDWithSubtype so it can be used over JNI. Performs disk/network operations on a
 * background thread and replies asynchronously.
 */
@JNINamespace("instance_id")
@NullMarked
public class InstanceIDBridge {
    private final String mSubtype;
    private final SequencedTaskRunner mTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    private long mNativeInstanceIDAndroid;

    private static boolean sBlockOnAsyncTasksForTesting;

    private InstanceIDBridge(long nativeInstanceIDAndroid, String subtype) {
        mSubtype = subtype;
        mNativeInstanceIDAndroid = nativeInstanceIDAndroid;
    }

    /**
     * Returns a wrapped {@link InstanceIDWithSubtype}. Multiple InstanceIDBridge instances may
     * share an underlying InstanceIDWithSubtype.
     */
    @CalledByNative
    public static InstanceIDBridge create(
            long nativeInstanceIDAndroid, @JniType("std::string") String subtype) {
        return new InstanceIDBridge(nativeInstanceIDAndroid, subtype);
    }

    /**
     * Called when our C++ counterpart is destroyed. Clears the handle to our native C++ object,
     * ensuring it's not called by pending async tasks.
     */
    @CalledByNative
    private void destroy() {
        mNativeInstanceIDAndroid = 0;
    }

    /**
     * If enabled, methods that are usually asynchronous will block returning the control flow to
     * C++ until the asynchronous Java operation has completed. Used this in tests where the Java
     * message loop is not nested. The caller is expected to reset this to false when tearing down.
     */
    @CalledByNative
    private static boolean setBlockOnAsyncTasksForTesting(boolean block) {
        boolean wasBlocked = sBlockOnAsyncTasksForTesting;
        sBlockOnAsyncTasksForTesting = block;
        return wasBlocked;
    }

    /** Async wrapper for {@link InstanceID#getId}. */
    @CalledByNative
    public void getId(final int requestId) {
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork(InstanceIDWithSubtype instanceId) {
                return instanceId.getId();
            }

            @Override
            protected void sendResultToNative(String id) {
                InstanceIDBridgeJni.get().didGetID(mNativeInstanceIDAndroid, requestId, id);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#getCreationTime}. */
    @CalledByNative
    public void getCreationTime(final int requestId) {
        new BridgeAsyncTask<Long>() {
            @Override
            protected Long doBackgroundWork(InstanceIDWithSubtype instanceId) {
                return instanceId.getCreationTime();
            }

            @Override
            protected void sendResultToNative(Long creationTime) {
                InstanceIDBridgeJni.get()
                        .didGetCreationTime(mNativeInstanceIDAndroid, requestId, creationTime);
            }
        }.execute();
    }

    /**
     * Async wrapper for {@link InstanceID#getToken(String, String)}. |isLazy| isn't part of the
     * InstanceID.getToken() call and not sent to the FCM server. It's used to mark the subscription
     * as lazy such that incoming messages are deferred until there are visible activities.
     */
    @CalledByNative
    @VisibleForTesting
    void getToken(
            final int requestId,
            final @JniType("std::string") String authorizedEntity,
            final @JniType("std::string") String scope,
            int flags) {
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork(InstanceIDWithSubtype instanceId) {
                try {
                    // TODO(crbug.com/40789764): Migrate stored LazySubscriptionsManager data to
                    // SubscriptionFlagManager.
                    LazySubscriptionsManager.storeLazinessInformation(
                            LazySubscriptionsManager.buildSubscriptionUniqueId(
                                    mSubtype, authorizedEntity),
                            (flags & InstanceIDFlags.IS_LAZY) == InstanceIDFlags.IS_LAZY);
                    SubscriptionFlagManager.setFlags(
                            SubscriptionFlagManager.buildSubscriptionUniqueId(
                                    mSubtype, authorizedEntity),
                            flags);
                    return instanceId.getToken(authorizedEntity, scope);
                } catch (IOException ex) {
                    return "";
                }
            }

            @Override
            protected void sendResultToNative(String token) {
                InstanceIDBridgeJni.get().didGetToken(mNativeInstanceIDAndroid, requestId, token);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteToken(String, String)}. */
    @CalledByNative
    private void deleteToken(
            final int requestId,
            final @JniType("std::string") String authorizedEntity,
            final @JniType("std::string") String scope) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork(InstanceIDWithSubtype instanceId) {
                try {
                    instanceId.deleteToken(authorizedEntity, scope);
                    String subscriptionId =
                            LazySubscriptionsManager.buildSubscriptionUniqueId(
                                    mSubtype, authorizedEntity);
                    if (LazySubscriptionsManager.isSubscriptionLazy(subscriptionId)) {
                        LazySubscriptionsManager.deletePersistedMessagesForSubscriptionId(
                                subscriptionId);
                    }
                    SubscriptionFlagManager.clearFlags(
                            SubscriptionFlagManager.buildSubscriptionUniqueId(
                                    mSubtype, authorizedEntity));
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }

            @Override
            protected void sendResultToNative(Boolean success) {
                InstanceIDBridgeJni.get()
                        .didDeleteToken(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteInstanceID}. */
    @CalledByNative
    @VisibleForTesting
    void deleteInstanceID(final int requestId) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork(InstanceIDWithSubtype instanceId) {
                try {
                    instanceId.deleteInstanceID();
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }

            @Override
            protected void sendResultToNative(Boolean success) {
                InstanceIDBridgeJni.get().didDeleteID(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    /**
     * Custom {@link AsyncTask} wrapper. As usual, this performs work on a background thread, then
     * sends the result back on the UI thread. Key differences:
     *
     * <p>1. Resolves {@link InstanceIDWithSubtype} on the background thread before running
     * doBackgroundWork, so a fresh instance is created after {@link #deleteInstanceID}.
     *
     * <p>2. sendResultToNative will be skipped if the C++ counterpart has been destroyed.
     *
     * <p>3. Tasks for the same {@link InstanceIDBridge} run sequentially on {@link #mTaskRunner} to
     * preserve call order (e.g. {@link #deleteInstanceID} followed by {@link #getToken}) without
     * blocking other Chrome tasks.
     *
     * <p>4. If setBlockOnAsyncTasksForTesting has been enabled, the work will skip the task runner
     * and run directly on the main thread. This is necessary because there are some complex
     * behaviors around executor lifecycle in unit tests.
     */
    private abstract class BridgeAsyncTask<Result> {
        protected abstract Result doBackgroundWork(InstanceIDWithSubtype instanceId);

        protected abstract void sendResultToNative(Result result);

        public void execute() {
            if (sBlockOnAsyncTasksForTesting) {
                sendResultToNative(doBackgroundWork(InstanceIDWithSubtype.getInstance(mSubtype)));
                return;
            }
            AsyncTask<Result> task =
                    new AsyncTask<>() {
                        @Override
                        protected Result doInBackground() {
                            return doBackgroundWork(InstanceIDWithSubtype.getInstance(mSubtype));
                        }

                        @Override
                        protected void onPostExecute(Result result) {
                            if (mNativeInstanceIDAndroid != 0) {
                                sendResultToNative(result);
                            }
                        }
                    };
            task.executeOnTaskRunner(mTaskRunner);
        }
    }

    @NativeMethods
    interface Natives {
        void didGetID(
                long nativeInstanceIDAndroid, int requestId, @JniType("std::string") String id);

        void didGetCreationTime(long nativeInstanceIDAndroid, int requestId, long creationTime);

        void didGetToken(
                long nativeInstanceIDAndroid, int requestId, @JniType("std::string") String token);

        void didDeleteToken(long nativeInstanceIDAndroid, int requestId, boolean success);

        void didDeleteID(long nativeInstanceIDAndroid, int requestId, boolean success);
    }
}
