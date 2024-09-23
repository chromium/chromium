// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.AsyncTask;
import org.chromium.components.gcm_driver.InstanceIDFlags;
import org.chromium.components.gcm_driver.LazySubscriptionsManager;
import org.chromium.components.gcm_driver.SubscriptionFlagManager;

import java.io.IOException;

/**
 * Wraps InstanceIDWithSubtype so it can be used over JNI.
 * Performs disk/network operations on a background thread and replies asynchronously.
 */
@JNINamespace("instance_id")
public class InstanceIDBridge {
    private final String mSubtype;
    private long mNativeInstanceIDAndroid;

    /**
     * Underlying InstanceIDWithSubtype. May be shared by multiple InstanceIDBridges. Must be
     * initialized on a background thread.
     */
    private InstanceIDWithSubtype mInstanceID;

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
    public static InstanceIDBridge create(long nativeInstanceIDAndroid, String subtype) {
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
            protected String doBackgroundWork() {
                return mInstanceID.getId();
            }

            @Override
            protected void sendResultToNative(String id) {
                InstanceIDBridgeJni.get()
                        .didGetID(mNativeInstanceIDAndroid, InstanceIDBridge.this, requestId, id);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#getCreationTime}. */
    @CalledByNative
    public void getCreationTime(final int requestId) {
        new BridgeAsyncTask<Long>() {
            @Override
            protected Long doBackgroundWork() {
                return mInstanceID.getCreationTime();
            }

            @Override
            protected void sendResultToNative(Long creationTime) {
                InstanceIDBridgeJni.get()
                        .didGetCreationTime(
                                mNativeInstanceIDAndroid,
                                InstanceIDBridge.this,
                                requestId,
                                creationTime);
            }
        }.execute();
    }

    /**
     * Async wrapper for {@link InstanceID#getToken(String, String)}.
     * |isLazy| isn't part of the InstanceID.getToken() call and not sent to the
     * FCM server. It's used to mark the subscription as lazy such that incoming
     * messages are deferred until there are visible activities.
     */
    @CalledByNative
    private void getToken(
            final int requestId, final String authorizedEntity, final String scope, int flags) {
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork() {
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
                    return mInstanceID.getToken(authorizedEntity, scope);
                } catch (IOException ex) {
                    return "";
                }
            }

            @Override
            protected void sendResultToNative(String token) {
                InstanceIDBridgeJni.get()
                        .didGetToken(
                                mNativeInstanceIDAndroid, InstanceIDBridge.this, requestId, token);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteToken(String, String)}. */
    @CalledByNative
    private void deleteToken(
            final int requestId, final String authorizedEntity, final String scope) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteToken(authorizedEntity, scope);
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
                        .didDeleteToken(
                                mNativeInstanceIDAndroid,
                                InstanceIDBridge.this,
                                requestId,
                                success);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteInstanceID}. */
    @CalledByNative
    private void deleteInstanceID(final int requestId) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteInstanceID();
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }

            @Override
            protected void sendResultToNative(Boolean success) {
                InstanceIDBridgeJni.get()
                        .didDeleteID(
                                mNativeInstanceIDAndroid,
                                InstanceIDBridge.this,
                                requestId,
                                success);
            }
        }.execute();
    }

    /**
     * Custom {@link AsyncTask} wrapper. As usual, this performs work on a background thread, then
     * sends the result back on the UI thread. Key differences:
     *
     * <p>1. Lazily initializes mInstanceID before running doBackgroundWork.
     *
     * <p>2. sendResultToNative will be skipped if the C++ counterpart has been destroyed.
     *
     * <p>3. Tasks run in parallel (using THREAD_POOL_EXECUTOR) to avoid blocking other Chrome
     * tasks.
     *
     * <p>4. If setBlockOnAsyncTasksForTesting has been enabled, the work will skip the thread pool
     * executor and run directly on the main thread. This is necessary because there are some
     * complex behaviors around executor lifecycle in unit tests.
     */
    private abstract class BridgeAsyncTask<Result> {
        protected abstract Result doBackgroundWork();

        protected abstract void sendResultToNative(Result result);

        public void execute() {
            if (sBlockOnAsyncTasksForTesting) {
                if (mInstanceID == null) {
                    mInstanceID = InstanceIDWithSubtype.getInstance(mSubtype);
                }
                sendResultToNative(doBackgroundWork());
                return;
            }
            AsyncTask<Result> task =
                    new AsyncTask<Result>() {
                        @Override
                        @SuppressWarnings(
                                "NoSynchronizedThisCheck") // Only used/accessible by native.
                        protected Result doInBackground() {
                            synchronized (InstanceIDBridge.this) {
                                if (mInstanceID == null) {
                                    mInstanceID = InstanceIDWithSubtype.getInstance(mSubtype);
                                }
                            }
                            return doBackgroundWork();
                        }

                        @Override
                        protected void onPostExecute(Result result) {
                            if (mNativeInstanceIDAndroid != 0) {
                                sendResultToNative(result);
                            }
                        }
                    };
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @NativeMethods
    interface Natives {
        void didGetID(
                long nativeInstanceIDAndroid, InstanceIDBridge caller, int requestId, String id);

        void didGetCreationTime(
                long nativeInstanceIDAndroid,
                InstanceIDBridge caller,
                int requestId,
                long creationTime);

        void didGetToken(
                long nativeInstanceIDAndroid, InstanceIDBridge caller, int requestId, String token);

        void didDeleteToken(
                long nativeInstanceIDAndroid,
                InstanceIDBridge caller,
                int requestId,
                boolean success);

        void didDeleteID(
                long nativeInstanceIDAndroid,
                InstanceIDBridge caller,
                int requestId,
                boolean success);
    }
}
