// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherTabId;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Implementation of {@link MessagingBackendService} that connects to the native counterpart. */
@JNINamespace("tab_groups::messaging::android")
/*package*/ class MessagingBackendServiceBridge implements MessagingBackendService {
    private static String getSyncId(EitherId id) {
        if (id == null || !id.isSyncId()) {
            return null;
        }
        return id.getSyncId();
    }

    private final ObserverList<PersistentMessageObserver> mPersistentMessageObservers =
            new ObserverList<>();

    private long mNativeMessagingBackendServiceBridge;
    private InstantMessageDelegate mInstantMessageDelegate;

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
                .isInitialized(mNativeMessagingBackendServiceBridge, this);
    }

    @Override
    public List<PersistentMessage> getMessagesForTab(
            EitherTabId tabId, Optional</* @PersistentNotificationType */ Integer> type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<PersistentMessage>();
        }

        Integer type_int;
        if (type == null || !type.isPresent()) {
            type_int = PersistentNotificationType.UNDEFINED;
        } else {
            type_int = type.get();
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
                        mNativeMessagingBackendServiceBridge,
                        this,
                        localTabId,
                        syncTabId,
                        type_int);
    }

    @Override
    public List<PersistentMessage> getMessagesForGroup(
            EitherGroupId groupId, Optional</* @PersistentNotificationType */ Integer> type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<PersistentMessage>();
        }

        Integer type_int;
        if (type == null || !type.isPresent()) {
            type_int = PersistentNotificationType.UNDEFINED;
        } else {
            type_int = type.get();
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
                        mNativeMessagingBackendServiceBridge,
                        this,
                        localGroupId,
                        syncGroupId,
                        type_int);
    }

    @Override
    public List<PersistentMessage> getMessages(
            Optional</* @PersistentNotificationType */ Integer> type) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<PersistentMessage>();
        }

        Integer type_int;
        if (type == null || !type.isPresent()) {
            type_int = PersistentNotificationType.UNDEFINED;
        } else {
            type_int = type.get();
        }

        return MessagingBackendServiceBridgeJni.get()
                .getMessages(mNativeMessagingBackendServiceBridge, this, type_int);
    }

    @Override
    public List<ActivityLogItem> getActivityLog(ActivityLogQueryParams params) {
        if (mNativeMessagingBackendServiceBridge == 0) {
            return new ArrayList<ActivityLogItem>();
        }

        return MessagingBackendServiceBridgeJni.get()
                .getActivityLog(mNativeMessagingBackendServiceBridge, this, params.collaborationId);
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
        if (mInstantMessageDelegate != null) {
            mInstantMessageDelegate.displayInstantaneousMessage(
                    message,
                    (Boolean success) -> {
                        assert success != null;
                        MessagingBackendServiceBridgeJni.get()
                                .runInstantaneousMessageSuccessCallback(
                                        mNativeMessagingBackendServiceBridge,
                                        this,
                                        nativeCallback,
                                        success);
                    });
        }
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(
                long nativeMessagingBackendServiceBridge, MessagingBackendServiceBridge caller);

        List<PersistentMessage> getMessagesForTab(
                long nativeMessagingBackendServiceBridge,
                MessagingBackendServiceBridge caller,
                int localTabId,
                String syncTabId,
                @PersistentNotificationType int type);

        List<PersistentMessage> getMessagesForGroup(
                long nativeMessagingBackendServiceBridge,
                MessagingBackendServiceBridge caller,
                LocalTabGroupId localGroupId,
                String syncGroupId,
                @PersistentNotificationType int type);

        List<PersistentMessage> getMessages(
                long nativeMessagingBackendServiceBridge,
                MessagingBackendServiceBridge caller,
                @PersistentNotificationType int type);

        List<ActivityLogItem> getActivityLog(
                long nativeMessagingBackendServiceBridge,
                MessagingBackendServiceBridge caller,
                String collaborationId);

        void runInstantaneousMessageSuccessCallback(
                long nativeMessagingBackendServiceBridge,
                MessagingBackendServiceBridge caller,
                long callback,
                boolean success);
    }
}
