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
import org.jni_zero.JniType;
import org.junit.Assert;
import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.components.tab_group_sync.EitherId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A companion object to the native MessagingBackendServiceBridgeTest. */
@JNINamespace("collaboration::messaging")
public class MessagingBackendServiceBridgeUnitTestCompanion {
    // The service instance we're testing.
    private final MessagingBackendService mService;

    private final MessagingBackendService.PersistentMessageObserver mObserver =
            mock(MessagingBackendService.PersistentMessageObserver.class);
    private final MessagingBackendService.InstantMessageDelegate mInstantMessageDelegate =
            mock(MessagingBackendService.InstantMessageDelegate.class);

    private final ArgumentCaptor<InstantMessage> mInstantMessageCaptor =
            ArgumentCaptor.forClass(InstantMessage.class);
    private final ArgumentCaptor<Callback> mInstantMessageCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);
    private final ArgumentCaptor<Set<String>> mHideInstantMessageIdsCaptor =
            ArgumentCaptor.forClass(Set.class);

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
            @JniType("collaboration::messaging::PersistentNotificationType")
                    @PersistentNotificationType
                    int type,
            int[] expectedCollaborationEvents) {
        List<PersistentMessage> messages = mService.getMessages(type);
        Assert.assertEquals(expectedCollaborationEvents.length, messages.size());
        for (int i = 0; i < expectedCollaborationEvents.length; ++i) {
            Assert.assertEquals(expectedCollaborationEvents[i], messages.get(i).collaborationEvent);
        }
    }

    @CalledByNative
    private void invokeGetMessagesForGroupAndVerify(
            LocalTabGroupId localGroupId,
            @Nullable String syncId,
            @JniType("collaboration::messaging::PersistentNotificationType")
                    @PersistentNotificationType
                    int type,
            int[] expectedCollaborationEvents) {
        EitherId.EitherGroupId groupId;
        if (syncId == null) {
            groupId = EitherId.EitherGroupId.createLocalId(localGroupId);
        } else {
            groupId = EitherId.EitherGroupId.createSyncId(syncId);
        }
        List<PersistentMessage> messages = mService.getMessagesForGroup(groupId, type);
        Assert.assertEquals(expectedCollaborationEvents.length, messages.size());
        for (int i = 0; i < expectedCollaborationEvents.length; ++i) {
            Assert.assertEquals(expectedCollaborationEvents[i], messages.get(i).collaborationEvent);
        }
    }

    @CalledByNative
    private void invokeClearDirtyTabMessagesForGroupAndVerify(
            LocalTabGroupId localGroupId, @Nullable String collaborationId) {
        mService.clearDirtyTabMessagesForGroup(collaborationId);
    }

    @CalledByNative
    private void invokeGetMessagesForTabAndVerify(
            int localTabId,
            @Nullable String syncId,
            @JniType("collaboration::messaging::PersistentNotificationType")
                    @PersistentNotificationType
                    int type,
            int[] expectedCollaborationEvents) {
        EitherId.EitherTabId tabId;
        if (syncId == null) {
            tabId = EitherId.EitherTabId.createLocalId(localTabId);
        } else {
            tabId = EitherId.EitherTabId.createSyncId(syncId);
        }
        List<PersistentMessage> messages = mService.getMessagesForTab(tabId, type);
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
        Assert.assertEquals("Message content - single message", message.localizedMessage);

        // MessageAttribution.
        Assert.assertTrue(message.isSingleMessage());
        MessageAttribution attribution = message.attributions.get(0);
        Assert.assertEquals("cf07d904-88d4-4bc9-989d-57a9ab9e17a7", attribution.id);
        Assert.assertEquals("my group", attribution.collaborationId);
        Assert.assertEquals(new GaiaId("affected"), attribution.affectedUser.gaiaId);
        Assert.assertEquals(new GaiaId("triggering"), attribution.triggeringUser.gaiaId);

        // TabGroupMessageMetadata.
        TabGroupMessageMetadata tgmm = attribution.tabGroupMetadata;
        Assert.assertEquals(
                new LocalTabGroupId(new Token(2748937106984275893L, 588177993057108452L)),
                tgmm.localTabGroupId);
        Assert.assertEquals("a1b2c3d4-e5f6-7890-1234-567890abcdef", tgmm.syncTabGroupId);
        Assert.assertEquals("last known group title", tgmm.lastKnownTitle);
        Assert.assertEquals(TabGroupColorId.ORANGE, tgmm.lastKnownColor.intValue());

        // TabMessageMetadata.
        TabMessageMetadata tmm = attribution.tabMetadata;
        Assert.assertEquals(499897179L, tmm.localTabId);
        Assert.assertEquals("fedcba09-8765-4321-0987-6f5e4d3c2b1a", tmm.syncTabId);
        Assert.assertEquals("https://example.com/", tmm.lastKnownUrl);
        Assert.assertEquals("last known tab title", tmm.lastKnownTitle);
    }

    @CalledByNative
    private void verifyAggregatedInstantMessage() {
        verify(mInstantMessageDelegate)
                .displayInstantaneousMessage(
                        mInstantMessageCaptor.capture(), mInstantMessageCallbackCaptor.capture());
        InstantMessage message = mInstantMessageCaptor.getValue();
        Assert.assertEquals(InstantNotificationLevel.SYSTEM, message.level);
        Assert.assertEquals(InstantNotificationType.CONFLICT_TAB_REMOVED, message.type);
        Assert.assertEquals(CollaborationEvent.TAB_REMOVED, message.collaborationEvent);
        Assert.assertEquals("Message content - aggregated message", message.localizedMessage);
        Assert.assertFalse(message.isSingleMessage());
        Assert.assertEquals(2, message.attributions.size());

        // Attribution 1.
        MessageAttribution attribution1 = message.attributions.get(0);
        Assert.assertEquals("cf07d904-88d4-4bc9-989d-57a9ab9e17a7", attribution1.id);
        Assert.assertEquals("my group", attribution1.collaborationId);
        Assert.assertEquals(new GaiaId("affected"), attribution1.affectedUser.gaiaId);
        Assert.assertEquals(new GaiaId("triggering"), attribution1.triggeringUser.gaiaId);

        // Attribution 2.
        MessageAttribution attribution2 = message.attributions.get(1);
        Assert.assertEquals("24ed7c34-41a3-47c2-aad4-5ea42a1765d5", attribution2.id);
        Assert.assertEquals("my group", attribution2.collaborationId);
        Assert.assertEquals(new GaiaId("affected 2"), attribution2.affectedUser.gaiaId);
        Assert.assertEquals(new GaiaId("triggering 2"), attribution2.triggeringUser.gaiaId);

        // TabGroupMessageMetadata of attribution 1.
        TabGroupMessageMetadata tgmm = attribution1.tabGroupMetadata;
        Assert.assertEquals(
                new LocalTabGroupId(new Token(2748937106984275893L, 588177993057108452L)),
                tgmm.localTabGroupId);
        Assert.assertEquals("a1b2c3d4-e5f6-7890-1234-567890abcdef", tgmm.syncTabGroupId);
        Assert.assertEquals("last known group title", tgmm.lastKnownTitle);
        Assert.assertEquals(TabGroupColorId.ORANGE, tgmm.lastKnownColor.intValue());

        // TabMessageMetadata of attribution 1.
        TabMessageMetadata tmm = attribution1.tabMetadata;
        Assert.assertEquals(499897179L, tmm.localTabId);
        Assert.assertEquals("fedcba09-8765-4321-0987-6f5e4d3c2b1a", tmm.syncTabId);
        Assert.assertEquals("https://example.com/", tmm.lastKnownUrl);
        Assert.assertEquals("last known tab title", tmm.lastKnownTitle);

        // TabMessageMetadata of attribution 2.
        Assert.assertEquals("last known tab title 2", attribution2.tabMetadata.lastKnownTitle);
    }

    @CalledByNative
    private void invokeInstantMessageSuccessCallback(boolean success) {
        mInstantMessageCallbackCaptor.getValue().onResult(success);
    }

    @CalledByNative
    private void verifyHideInstantMessageCalledWithIds(String[] expectedIdsArray) {
        verify(mInstantMessageDelegate)
                .hideInstantaneousMessage(mHideInstantMessageIdsCaptor.capture());
        Set<String> actualIds = mHideInstantMessageIdsCaptor.getValue();
        Assert.assertEquals(
                "Number of hidden IDs does not match", expectedIdsArray.length, actualIds.size());
        // Convert String[] to Set<String> for proper comparison, as order doesn't matter in Set.
        Set<String> expectedIdsSet = new HashSet<>(Arrays.asList(expectedIdsArray));
        Assert.assertEquals("Hidden message IDs do not match", expectedIdsSet, actualIds);
    }

    @CalledByNative
    private void invokeGetActivityLogAndVerify() {
        ActivityLogQueryParams queryParams = new ActivityLogQueryParams();
        queryParams.collaborationId = "collaboration1";
        List<ActivityLogItem> logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(2, logItems.size());

        Assert.assertEquals(CollaborationEvent.TAB_UPDATED, logItems.get(0).collaborationEvent);
        Assert.assertEquals("User 1", logItems.get(0).titleText);
        Assert.assertEquals("https://google.com", logItems.get(0).descriptionText);
        Assert.assertEquals("2 hours ago", logItems.get(0).timeDeltaText);
        Assert.assertTrue(logItems.get(0).showFavicon);
        Assert.assertEquals(RecentActivityAction.REOPEN_TAB, logItems.get(0).action);
        Assert.assertEquals(
                "1b687a61-8a17-4f98-bf9d-74d2b50abf3e", logItems.get(0).activityMetadata.id);

        Assert.assertEquals(
                CollaborationEvent.COLLABORATION_MEMBER_ADDED, logItems.get(1).collaborationEvent);
        Assert.assertEquals("User 2", logItems.get(1).titleText);
        Assert.assertEquals("foo@gmail.com", logItems.get(1).descriptionText);
        Assert.assertEquals("3 days ago", logItems.get(1).timeDeltaText);
        Assert.assertFalse(logItems.get(1).showFavicon);
        Assert.assertEquals(RecentActivityAction.MANAGE_SHARING, logItems.get(1).action);
        Assert.assertEquals(null, logItems.get(1).activityMetadata.id);

        queryParams.collaborationId = "collaboration2";
        logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(0, logItems.size());
    }
}
