// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.fcm;

import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/**
 * Java bridge for FcmDriverAndroid, managing JNI communication between C++ and Android
 * FCM/Installation services. Performs disk/network operations on a background thread and replies
 * asynchronously.
 */
@JNINamespace("fcm")
@NullMarked
public class FcmBridge {
    private static @Nullable FcmBridge sInstance;

    private long mNativeFcmDriverAndroid;

    private FcmBridge(long nativeFcmDriverAndroid) {
        mNativeFcmDriverAndroid = nativeFcmDriverAndroid;
    }

    /**
     * Create a FcmBridge object, which is owned by FcmDriverAndroid on the C++ side.
     *
     * @param nativeFcmDriverAndroid The C++ object that owns us.
     */
    @CalledByNative
    static FcmBridge create(long nativeFcmDriverAndroid) {
        assert sInstance == null : "Already instantiated";
        sInstance = new FcmBridge(nativeFcmDriverAndroid);
        return sInstance;
    }

    /** Sets a test instance. */
    public static void setInstanceForTesting(@Nullable FcmBridge testInstance) {
        var previous = sInstance;
        sInstance = testInstance;
        ResettersForTesting.register(() -> sInstance = previous);
    }

    /**
     * Called when our C++ counterpart is deleted. Clear the handle to our native C++ object,
     * ensuring it's never called.
     */
    @CalledByNative
    void destroy() {
        assert sInstance == this;
        sInstance = null;
        mNativeFcmDriverAndroid = 0;
    }

    /** Returns the global FcmBridge instance if instantiated, or null otherwise. */
    public static @Nullable FcmBridge getInstance() {
        return sInstance;
    }

    /** Initiates asynchronous fetching of the Installation ID on a background worker thread. */
    @CalledByNative
    public void fetchInstallationId() {
        PostTask.postTask(TaskTraits.USER_VISIBLE_MAY_BLOCK, this::fetchInstallationIdInBackground);
    }

    @WorkerThread
    private void fetchInstallationIdInBackground() {
        ThreadUtils.assertOnBackgroundThread();
        FcmManager.getInstance().fetchInstallationId(this::onInstallationIdFetched);
    }

    private void onInstallationIdFetched(String installationId) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mNativeFcmDriverAndroid != 0) {
                        FcmBridgeJni.get()
                                .onInstallationIdRefreshed(mNativeFcmDriverAndroid, installationId);
                    }
                });
    }

    /**
     * Called when an FCM message is received with a map of key-value data.
     *
     * @param messageId Unique message ID from FCM.
     * @param data Key-value data map.
     * @param rawData Raw binary payload.
     */
    public void onMessageReceived(String messageId, Map<String, String> data, byte[] rawData) {
        // TODO(b/546476623): When Android is not in foreground mode, native library may not be
        // loaded or accessible. Handle background message dispatching or native initialization.
        if (mNativeFcmDriverAndroid == 0) return;

        FcmBridgeJni.get().onMessageReceived(mNativeFcmDriverAndroid, messageId, data, rawData);
    }

    /** Called when messages are deleted on the server. */
    public void onMessagesDeleted() {
        if (mNativeFcmDriverAndroid == 0) return;
        FcmBridgeJni.get().onMessagesDeleted(mNativeFcmDriverAndroid);
    }

    @NativeMethods
    interface Natives {
        void onInstallationIdRefreshed(
                long nativeFcmDriverAndroid, @JniType("std::string") String installationId);

        void onMessageReceived(
                long nativeFcmDriverAndroid,
                @JniType("std::string") String messageId,
                @JniType("std::map<std::string, std::string>") Map<String, String> data,
                @JniType("std::vector<uint8_t>") byte[] rawData);

        void onMessagesDeleted(long nativeFcmDriverAndroid);
    }
}
