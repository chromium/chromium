// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.io.IOException;
import java.util.Set;

/**
 * This class is the Java counterpart to the C++ GCMDriverAndroid class.
 * It uses Android's Java GCM APIs to implement GCM registration etc, and
 * sends back GCM messages over JNI.
 *
 * Threading model: all calls to/from C++ happen on the UI thread.
 */
@JNINamespace("gcm")
public class GCMDriver {
    private static final String TAG = "GCMDriver";

    // The instance of GCMDriver currently owned by a C++ GCMDriverAndroid, if any.
    private static GCMDriver sInstance;

    private long mNativeGCMDriverAndroid;
    private GoogleCloudMessagingSubscriber mSubscriber;

    private GCMDriver(long nativeGCMDriverAndroid) {
        mNativeGCMDriverAndroid = nativeGCMDriverAndroid;
        mSubscriber = new GoogleCloudMessagingV2();
    }

    /**
     * Create a GCMDriver object, which is owned by GCMDriverAndroid
     * on the C++ side.
     *  @param nativeGCMDriverAndroid The C++ object that owns us.
     *
     */
    @CalledByNative
    private static GCMDriver create(long nativeGCMDriverAndroid) {
        if (sInstance != null) {
            throw new IllegalStateException("Already instantiated");
        }
        sInstance = new GCMDriver(nativeGCMDriverAndroid);
        // TODO(crbug.com/946486): This has been in added in M75 to migrate the
        // way we store if there are persisted messages. It should be removed in
        // M77.
        LazySubscriptionsManager.migrateHasPersistedMessagesPref();
        return sInstance;
    }

    /**
     * Called when our C++ counterpart is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
        mNativeGCMDriverAndroid = 0;
    }

    @CalledByNative
    private void replayPersistedMessages(final String appId) {
        Set<String> subscriptionsWithPersistedMessagesForAppId =
                LazySubscriptionsManager.getSubscriptionIdsWithPersistedMessages(appId);
        if (subscriptionsWithPersistedMessagesForAppId.isEmpty()) {
            return;
        }

        long time = SystemClock.elapsedRealtime();
        for (String id : subscriptionsWithPersistedMessagesForAppId) {
            GCMMessage[] messages = LazySubscriptionsManager.readMessages(id);
            for (GCMMessage message : messages) {
                dispatchMessage(message);
            }
            LazySubscriptionsManager.deletePersistedMessagesForSubscriptionId(id);
        }
        long duration = SystemClock.elapsedRealtime() - time;
        // Call RecordHistogram.recordTimesHistogram() on a background thread to avoid
        // expensive JNI calls in the critical path.
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            RecordHistogram.recordTimesHistogram(
                    "PushMessaging.TimeToReadPersistedMessages", duration);
        });
    }

    @CalledByNative
    private void register(final String appId, final String senderId) {
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                try {
                    String subtype = appId;
                    String registrationId = mSubscriber.subscribe(senderId, subtype, null);
                    return registrationId;
                } catch (IOException ex) {
                    Log.w(TAG, "GCM subscription failed for " + appId + ", " + senderId, ex);
                    return "";
                }
            }
            @Override
            protected void onPostExecute(String registrationId) {
                GCMDriverJni.get().onRegisterFinished(mNativeGCMDriverAndroid, GCMDriver.this,
                        appId, registrationId, !registrationId.isEmpty());
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @CalledByNative
    private void unregister(final String appId, final String senderId) {
        new AsyncTask<Boolean>() {
            @Override
            protected Boolean doInBackground() {
                try {
                    String subtype = appId;
                    mSubscriber.unsubscribe(senderId, subtype, null);
                    return true;
                } catch (IOException ex) {
                    Log.w(TAG, "GCM unsubscription failed for " + appId + ", " + senderId, ex);
                    return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean success) {
                GCMDriverJni.get().onUnregisterFinished(
                        mNativeGCMDriverAndroid, GCMDriver.this, appId, success);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    // The caller of this function is responsible for ensuring the browser process is initialized.
    public static void dispatchMessage(GCMMessage message) {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            throw new RuntimeException("Failed to instantiate GCMDriver.");
        }

        GCMDriverJni.get().onMessageReceived(sInstance.mNativeGCMDriverAndroid, sInstance,
                message.getAppId(), message.getSenderId(), message.getMessageId(),
                message.getCollapseKey(), message.getRawData(),
                message.getDataKeysAndValuesArray());
    }

    @VisibleForTesting
    public static void overrideSubscriberForTesting(GoogleCloudMessagingSubscriber subscriber) {
        assert sInstance != null;
        assert subscriber != null;
        sInstance.mSubscriber = subscriber;
    }

    @NativeMethods
    interface Natives {
        void onRegisterFinished(long nativeGCMDriverAndroid, GCMDriver caller, String appId,
                String registrationId, boolean success);
        void onUnregisterFinished(
                long nativeGCMDriverAndroid, GCMDriver caller, String appId, boolean success);
        void onMessageReceived(long nativeGCMDriverAndroid, GCMDriver caller, String appId,
                String senderId, String messageId, String collapseKey, byte[] rawData,
                String[] dataKeysAndValues);
    }
}
