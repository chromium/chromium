// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Looper;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;
import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;
import java.util.Optional;

/** A companion object to the native MessagingBackendServiceBridgeTest. */
@JNINamespace("collaboration::messaging")
public class MessagingBackendServiceBridgeUnitTestCompanion {
    // The service instance we're testing.
    private MessagingBackendService mService;

    private MessagingBackendService.PersistentMessageObserver mObserver =
            mock(MessagingBackendService.PersistentMessageObserver.class);
    private MessagingBackendService.InstantMessageDelegate mInstantMessageDelegate =
            mock(MessagingBackendService.InstantMessageDelegate.class);

    private ArgumentCaptor<InstantMessage> mInstantMessageCaptor =
            ArgumentCaptor.forClass(InstantMessage.class);
    private ArgumentCaptor<Callback> mInstantMessageCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);

    @CalledByNative
    private MessagingBackendServiceBridgeUnitTestCompanion(MessagingBackendService service) {
        mService = service;
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private boolean isInitialized() {
        return mService.isInitialized();
    }

    @CalledByNative
    private void addPersistentMessageObserver() {
        mService.addPersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void removePersistentMessageObserver() {
        mService.removePersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void verifyOnInitializedCalled(int times) {
        verify(mObserver, times(times)).onMessagingBackendServiceInitialized();
    }

    @CalledByNative
    private void setInstantMessageDelegate() {
        mService.setInstantMessageDelegate(mInstantMessageDelegate);
    }

    @CalledByNative
    private void invokeGetMessagesAndVerify(
            @PersistentNotificationType int type, int[] expectedCollaborationEvents) {
        List<PersistentMessage> messages;
        if (type == -1) {
            messages = mService.getMessages(Optional.empty());
        } else {
            messages = mService.getMessages(Optional.of(type));
        }
        Assert.assertEquals(expectedCollaborationEvents.length, messages.size());
        for (int i = 0; i < expectedCollaborationEvents.length; ++i) {
            Assert.assertEquals(expectedCollaborationEvents[i], messages.get(i).collaborationEvent);
        }
    }

    @CalledByNative
    private void invokeGetMessagesForGroupAndVerify(
            LocalTabGroupId localGroupId,
            @Nullable String syncId,
            @PersistentNotificationType int type,
            int[] expectedCollaborationEvents) {
        EitherId.EitherGroupId groupId;
        if (syncId == null) {
            groupId = EitherId.EitherGroupId.createLocalId(localGroupId);
        } else {
            groupId = EitherId.EitherGroupId.createSyncId(syncId);
        }
        List<PersistentMessage> messages;
        if (type == -1) {
            messages = mService.getMessagesForGroup(groupId, Optional.empty());
        } else {
            messages = mService.getMessagesForGroup(groupId, Optional.of(type));
        }
        Assert.assertEquals(expectedCollaborationEvents.length, messages.size());
        for (int i = 0; i < expectedCollaborationEvents.length; ++i) {
            Assert.assertEquals(expectedCollaborationEvents[i], messages.get(i).collaborationEvent);
        }
    }

    @CalledByNative
    private void invokeGetMessagesForTabAndVerify(
            int localTabId,
            @Nullable String syncId,
            @PersistentNotificationType int type,
            int[] expectedCollaborationEvents) {
        EitherId.EitherTabId tabId;
        if (syncId == null) {
            tabId = EitherId.EitherTabId.createLocalId(localTabId);
        } else {
            tabId = EitherId.EitherTabId.createSyncId(syncId);
        }
        List<PersistentMessage> messages;
        if (type == -1) {
            messages = mService.getMessagesForTab(tabId, Optional.empty());
        } else {
            messages = mService.getMessagesForTab(tabId, Optional.of(type));
        }
        Assert.assertEquals(expectedCollaborationEvents.length, messages.size());
        for (int i = 0; i < expectedCollaborationEvents.length; ++i) {
            Assert.assertEquals(expectedCollaborationEvents[i], messages.get(i).collaborationEvent);
        }
    }

    @CalledByNative
    private void verifyInstantMessage() {
        verify(mInstantMessageDelegate)
                .displayInstantaneousMessage(
                        mInstantMessageCaptor.capture(), mInstantMessageCallbackCaptor.capture());
        InstantMessage message = mInstantMessageCaptor.getValue();
        Assert.assertEquals(InstantNotificationLevel.SYSTEM, message.level);
        Assert.assertEquals(InstantNotificationType.CONFLICT_TAB_REMOVED, message.type);
        Assert.assertEquals(CollaborationEvent.TAB_REMOVED, message.collaborationEvent);

        // MessageAttribution.
        MessageAttribution attribution = message.attribution;
        Assert.assertEquals("my group", attribution.collaborationId);
        Assert.assertEquals("affected", attribution.affectedUser.gaiaId);
        Assert.assertEquals("triggering", attribution.triggeringUser.gaiaId);

        // TabGroupMessageMetadata.
        TabGroupMessageMetadata tgmm = attribution.tabGroupMetadata;
        Assert.assertEquals(
                new LocalTabGroupId(new Token(2748937106984275893L, 588177993057108452L)),
                tgmm.localTabGroupId);
        Assert.assertEquals("a1b2c3d4-e5f6-7890-1234-567890abcdef", tgmm.syncTabGroupId);
        Assert.assertEquals("last known group title", tgmm.lastKnownTitle);
        Assert.assertEquals(TabGroupColorId.ORANGE, tgmm.lastKnownColor.get().intValue());

        // TabMessageMetadata.
        TabMessageMetadata tmm = attribution.tabMetadata;
        Assert.assertEquals(499897179L, tmm.localTabId);
        Assert.assertEquals("fedcba09-8765-4321-0987-6f5e4d3c2b1a", tmm.syncTabId);
        Assert.assertEquals("https://example.com/", tmm.lastKnownUrl);
        Assert.assertEquals("last known tab title", tmm.lastKnownTitle);
    }

    @CalledByNative
    private void invokeInstantMessageSuccessCallback(boolean success) {
        mInstantMessageCallbackCaptor.getValue().onResult(success);
    }

    @CalledByNative
    private void invokeGetActivityLogAndVerify() {
        ActivityLogQueryParams queryParams = new ActivityLogQueryParams();
        queryParams.collaborationId = "collaboration1";
        List<ActivityLogItem> logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(2, logItems.size());

        Assert.assertEquals(CollaborationEvent.TAB_UPDATED, logItems.get(0).collaborationEvent);
        Assert.assertEquals("title 1", logItems.get(0).titleText);
        Assert.assertEquals("description 1", logItems.get(0).descriptionText);
        Assert.assertEquals("timestamp 1", logItems.get(0).timestampText);

        Assert.assertEquals(
                CollaborationEvent.COLLABORATION_MEMBER_ADDED, logItems.get(1).collaborationEvent);
        Assert.assertEquals("title 2", logItems.get(1).titleText);
        Assert.assertEquals("description 2", logItems.get(1).descriptionText);
        Assert.assertEquals("timestamp 2", logItems.get(1).timestampText);

        queryParams.collaborationId = "collaboration2";
        logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(0, logItems.size());
    }
}
