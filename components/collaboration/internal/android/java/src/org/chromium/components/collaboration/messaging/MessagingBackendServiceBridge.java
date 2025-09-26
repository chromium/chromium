// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_group_sync.EitherId;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.EitherId.EitherTabId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Implementation of {@link MessagingBackendService} that connects to the native counterpart. */
@JNINamespace("collaboration::messaging::android")
@NullMarked
/*package*/ class MessagingBackendServiceBridge implements MessagingBackendService {
    private static @Nullable String getSyncId(@Nullable EitherId id) {
        if (id == null || !id.isSyncId()) {
            return null;
        }
        return id.getSyncId();
    }

    private final ObserverList<PersistentMessageObserver> mPersistentMessageObservers =
            new ObserverList<>();

    private long mNativeMessagingBackendServiceBridge;
    private @Nullable InstantMessageDelegate mInstantMessageDelegate;

    private MessagingBackendServiceBridge(long nativeMessagingBackendServiceBridge) {
        mNativeMessagingBackendServiceBridge = nativeMessagingBackendServiceBridge;
    }

    // MessagingBackendService implementation.
    @Override
    public void setInstantMessageDelegate(InstantMessageDelegate delegate) {
        mInstantMessageDelegate = delegate;
    }

    @Override
    public void addPersistentMessageObserver(PersistentMessageObserver observer) {
        mPersistentMessageObservers.addObserver(observer);
    }

    @Override
    public void removePersistentMessageObserver(PersistentMessageObserver observer) {
        mPersistentMessageObservers.removeObserver(observer);
    }

    @Override
    public boolean isInitialized() {
        return MessagingBackendServiceBridgeJni.get()
                .isInitialized(mNativeMessagingBackendServiceBridge);
    }

    @Override
    public List<PersistentMessage> getMessagesForTab(
            @Nullable EitherTabId tabId, @PersistentNotificationType int type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<>();
        }

        int localTabId;
        if (tabId == null || !tabId.isLocalId()) {
            localTabId = EitherTabId.INVALID_TAB_ID;
        } else {
            localTabId = tabId.getLocalId();
        }
        String syncTabId = getSyncId(tabId);

        return MessagingBackendServiceBridgeJni.get()
                .getMessagesForTab(
                        mNativeMessagingBackendServiceBridge, localTabId, syncTabId, type);
    }

    @Override
    public List<PersistentMessage> getMessagesForGroup(
            @Nullable EitherGroupId groupId, @PersistentNotificationType int type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<>();
        }

        LocalTabGroupId localGroupId;
        if (groupId == null || !groupId.isLocalId()) {
            localGroupId = null;
        } else {
            localGroupId = groupId.getLocalId();
        }
        String syncGroupId = getSyncId(groupId);

        return MessagingBackendServiceBridgeJni.get()
                .getMessagesForGroup(
                        mNativeMessagingBackendServiceBridge, localGroupId, syncGroupId, type);
    }

    @Override
    public List<PersistentMessage> getMessages(@PersistentNotificationType int type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return Collections.emptyList();
        }

        return MessagingBackendServiceBridgeJni.get()
                .getMessages(mNativeMessagingBackendServiceBridge, type);
    }

    @Override
    public List<ActivityLogItem> getActivityLog(ActivityLogQueryParams params) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<>();
        }

        return MessagingBackendServiceBridgeJni.get()
                .getActivityLog(mNativeMessagingBackendServiceBridge, params.collaborationId);
    }

    @Override
    public void clearDirtyTabMessagesForGroup(String collaborationId) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return;
        }

        MessagingBackendServiceBridgeJni.get()
                .clearDirtyTabMessagesForGroup(
                        mNativeMessagingBackendServiceBridge, collaborationId);
    }

    @Override
    public void clearPersistentMessage(String messageId, @PersistentNotificationType int type) {
        MessagingBackendServiceBridgeJni.get()
                .clearPersistentMessage(mNativeMessagingBackendServiceBridge, messageId, type);
    }

    @CalledByNative
    private static MessagingBackendServiceBridge create(long nativeMessagingBackendServiceBridge) {
        return new MessagingBackendServiceBridge(nativeMessagingBackendServiceBridge);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeMessagingBackendServiceBridge = 0;
    }

    @CalledByNative
    private void onMessagingBackendServiceInitialized() {
        for (PersistentMessageObserver observer : mPersistentMessageObservers) {
            observer.onMessagingBackendServiceInitialized();
        }
    }

    @CalledByNative
    private void displayPersistentMessage(PersistentMessage message) {
        for (PersistentMessageObserver observer : mPersistentMessageObservers) {
            observer.displayPersistentMessage(message);
        }
    }

    @CalledByNative
    private void hidePersistentMessage(PersistentMessage message) {
        for (PersistentMessageObserver observer : mPersistentMessageObservers) {
            observer.hidePersistentMessage(message);
        }
    }

    @CalledByNative
    private void displayInstantaneousMessage(InstantMessage message, long nativeCallback) {
        if (mInstantMessageDelegate == null) {
            MessagingBackendServiceBridgeJni.get()
                    .runInstantaneousMessageSuccessCallback(
                            mNativeMessagingBackendServiceBridge, nativeCallback, false);
            return;
        }

        mInstantMessageDelegate.displayInstantaneousMessage(
                message,
                (Boolean success) -> {
                    assert success != null;
                    MessagingBackendServiceBridgeJni.get()
                            .runInstantaneousMessageSuccessCallback(
                                    mNativeMessagingBackendServiceBridge, nativeCallback, success);
                });
    }

    @CalledByNative
    private void hideInstantaneousMessage(Set<String> messageIds) {
        if (mInstantMessageDelegate == null) {
            return;
        }
        mInstantMessageDelegate.hideInstantaneousMessage(messageIds);
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(long nativeMessagingBackendServiceBridge);

        List<PersistentMessage> getMessagesForTab(
                long nativeMessagingBackendServiceBridge,
                int localTabId,
                @Nullable String syncTabId,
                @PersistentNotificationType int type);

        List<PersistentMessage> getMessagesForGroup(
                long nativeMessagingBackendServiceBridge,
                @Nullable LocalTabGroupId localGroupId,
                @Nullable String syncGroupId,
                @PersistentNotificationType int type);

        List<PersistentMessage> getMessages(
                long nativeMessagingBackendServiceBridge, @PersistentNotificationType int type);

        List<ActivityLogItem> getActivityLog(
                long nativeMessagingBackendServiceBridge, String collaborationId);

        void clearDirtyTabMessagesForGroup(
                long nativeMessagingBackendServiceBridge, String collaborationId);

        void runInstantaneousMessageSuccessCallback(
                long nativeMessagingBackendServiceBridge, long callback, boolean success);

        void clearPersistentMessage(
                long nativeMessagingBackendServiceBridge,
                String messageId,
                @PersistentNotificationType int type);
    }
}
