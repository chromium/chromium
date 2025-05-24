// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging.bridge;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.InstantMessage;
import org.chromium.components.collaboration.messaging.InstantNotificationLevel;
import org.chromium.components.collaboration.messaging.InstantNotificationType;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.RecentActivityAction;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.TreeSet;

/**
 * Helper class meant to be called by native. Used to create Java objects from C++ objects. Do not
 * call these methods directly.
 */
@JNINamespace("collaboration::messaging::android")
@NullMarked
class ConversionUtils {
    @CalledByNative
    private static MessageAttribution createAttributionFrom(
            String id,
            String collaborationId,
            @Nullable LocalTabGroupId localTabGroupId,
            @Nullable String syncTabGroupId,
            @Nullable String lastKnownTabGroupTitle,
            @TabGroupColorId int lastKnownTabGroupColor,
            int localTabId,
            @Nullable String syncTabId,
            @Nullable String lastKnownTabTitle,
            @Nullable String lastKnownTabUrl,
            @Nullable String previousTabUrl,
            @Nullable GroupMember affectedUser,
            boolean affectedUserIsSelf,
            GroupMember triggeringUser,
            boolean triggeringUserIsSelf) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.id = TextUtils.isEmpty(id) ? null : id;
        attribution.collaborationId = collaborationId;
        if (localTabGroupId != null
                || syncTabGroupId != null
                || lastKnownTabGroupTitle != null
                || lastKnownTabGroupColor != -1) {
            attribution.tabGroupMetadata = new TabGroupMessageMetadata();
            attribution.tabGroupMetadata.localTabGroupId = localTabGroupId;
            attribution.tabGroupMetadata.syncTabGroupId = syncTabGroupId;
            attribution.tabGroupMetadata.lastKnownTitle = lastKnownTabGroupTitle;
            if (lastKnownTabGroupColor == -1) {
                attribution.tabGroupMetadata.lastKnownColor = Optional.empty();
            } else {
                attribution.tabGroupMetadata.lastKnownColor = Optional.of(lastKnownTabGroupColor);
            }
        }
        if (localTabId != -1
                || syncTabId != null
                || lastKnownTabTitle != null
                || lastKnownTabUrl != null) {
            attribution.tabMetadata = new TabMessageMetadata();
            attribution.tabMetadata.localTabId = localTabId;
            attribution.tabMetadata.syncTabId = syncTabId;
            attribution.tabMetadata.lastKnownTitle = lastKnownTabTitle;
            attribution.tabMetadata.lastKnownUrl = lastKnownTabUrl;
            attribution.tabMetadata.previousUrl = previousTabUrl;
        }
        attribution.affectedUser = affectedUser;
        attribution.affectedUserIsSelf = affectedUserIsSelf;
        attribution.triggeringUser = triggeringUser;
        attribution.triggeringUserIsSelf = triggeringUserIsSelf;
        return attribution;
    }

    @CalledByNative
    private static ArrayList<PersistentMessage> createPersistentMessageList() {
        return new ArrayList<PersistentMessage>();
    }

    @CalledByNative
    private static PersistentMessage createPersistentMessageAndMaybeAddToList(
            @Nullable ArrayList<PersistentMessage> list,
            MessageAttribution attribution,
            @CollaborationEvent int collaborationEvent,
            @PersistentNotificationType int type) {
        PersistentMessage message = new PersistentMessage();
        message.attribution = attribution;
        message.collaborationEvent = collaborationEvent;
        message.type = type;

        if (list != null) {
            list.add(message);
        }

        return message;
    }

    @CalledByNative
    private static InstantMessage createInstantMessage(
            @CollaborationEvent int collaborationEvent,
            @InstantNotificationLevel int level,
            @InstantNotificationType int type,
            String localizedMessage,
            List<MessageAttribution> attributions) {
        InstantMessage message = new InstantMessage();
        message.collaborationEvent = collaborationEvent;
        message.level = level;
        message.type = type;
        message.localizedMessage = localizedMessage;
        message.attributions = attributions;

        return message;
    }

    @CalledByNative
    private static Set<String> createStringSet() {
        return new TreeSet<String>();
    }

    @CalledByNative
    private static void addStringToStringSet(Set<String> set, String string) {
        set.add(string);
    }

    @CalledByNative
    private static List<MessageAttribution> addAttributionToList(
            @Nullable List<MessageAttribution> attributions, MessageAttribution attribution) {
        if (attributions == null) {
            attributions = new ArrayList<>();
        }
        attributions.add(attribution);
        return attributions;
    }

    @CalledByNative
    private static ArrayList<ActivityLogItem> createActivityLogItemList() {
        return new ArrayList<ActivityLogItem>();
    }

    @CalledByNative
    private static ActivityLogItem createActivityLogItemAndMaybeAddToList(
            @Nullable ArrayList<ActivityLogItem> list,
            @CollaborationEvent int collaborationEvent,
            String titleText,
            String descriptionText,
            String timeDeltaText,
            boolean showFavicon,
            @RecentActivityAction int action,
            MessageAttribution activityMetadata) {
        ActivityLogItem activityLogItem = new ActivityLogItem();
        activityLogItem.collaborationEvent = collaborationEvent;
        activityLogItem.titleText = titleText;
        activityLogItem.descriptionText = descriptionText;
        activityLogItem.timeDeltaText = timeDeltaText;
        activityLogItem.showFavicon = showFavicon;
        activityLogItem.action = action;
        activityLogItem.activityMetadata = activityMetadata;

        if (list != null) {
            list.add(activityLogItem);
        }

        return activityLogItem;
    }
}
