// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.fcm;

import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
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

    @CalledByNative
    static FcmBridge create(long nativeFcmDriverAndroid) {
        assert sInstance == null : "FcmBridge is already created";
        FcmBridge bridge = new FcmBridge(nativeFcmDriverAndroid);
        sInstance = bridge;
        return bridge;
    }

    private FcmBridge(long nativeFcmDriverAndroid) {
        mNativeFcmDriverAndroid = nativeFcmDriverAndroid;
    }

    @CalledByNative
    void destroy() {
        mNativeFcmDriverAndroid = 0;
        if (sInstance == this) {
            sInstance = null;
        }
    }

    /** Returns the active FcmBridge singleton instance if created by native. */
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
     * Called when an FCM message is received.
     *
     * @param messageId Unique message ID from FCM.
     * @param data Key-value data map.
     * @param rawData Optional raw binary payload.
     */
    public void onMessageReceived(
            @Nullable String messageId,
            @Nullable Map<String, String> data,
            byte @Nullable [] rawData) {
        // TODO(b/546476623): When Android is not in foreground mode, native library may not be
        // loaded or accessible. Handle background message dispatching or native initialization.
        if (mNativeFcmDriverAndroid == 0) return;

        List<String> keysAndValues = new ArrayList<>();
        if (data != null) {
            for (Map.Entry<String, String> entry : data.entrySet()) {
                keysAndValues.add(entry.getKey());
                keysAndValues.add(entry.getValue());
            }
        }

        FcmBridgeJni.get()
                .onMessageReceived(
                        mNativeFcmDriverAndroid,
                        messageId != null ? messageId : "",
                        keysAndValues.toArray(new String[0]),
                        rawData != null ? rawData : new byte[0]);
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
                @JniType("std::vector<std::string>") String[] dataKeysAndValues,
                @JniType("std::vector<uint8_t>") byte[] rawData);

        void onMessagesDeleted(long nativeFcmDriverAndroid);
    }
}
