// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupMember;

/** Provides functions to safely read fields out of messages, performing null checks. */
@NullMarked
public class MessageUtils {

    /** No instantiation. */
    private MessageUtils() {}

    /** Returns the id of the tab, or -1. */
    public static int extractTabId(@Nullable PersistentMessage message) {
        return message == null
                        || message.attribution == null
                        || message.attribution.tabMetadata == null
                ? TabMessageMetadata.INVALID_TAB_ID
                : message.attribution.tabMetadata.localTabId;
    }

    /** Returns the id of the group, or null. */
    public static @Nullable Token extractTabGroupId(@Nullable PersistentMessage message) {
        return message == null ? null : extractTabGroupId(message.attribution);
    }

    /** Returns the id of the group, or null. */
    public static @Nullable MessageAttribution getFirstAttribution(
            @Nullable InstantMessage message) {
        return message == null
                ? null
                : (message.attributions.isEmpty() ? null : message.attributions.get(0));
    }

    /** Returns the id of the group, or null. */
    public static @Nullable Token extractTabGroupId(@Nullable InstantMessage message) {
        return message == null ? null : extractTabGroupId(getFirstAttribution(message));
    }

    /** Returns the sync id of the group, or null. */
    public static @Nullable String extractSyncTabGroupId(@Nullable InstantMessage message) {
        return message == null ? null : extractSyncTabGroupId(getFirstAttribution(message));
    }

    /** Returns the sync id of the group, or null. */
    public static @Nullable String extractSyncTabGroupId(@Nullable PersistentMessage message) {
        return message == null ? null : extractSyncTabGroupId(message.attribution);
    }

    /** Returns the tab group title or the empty string. */
    public static @Nullable String extractTabGroupTitle(@Nullable PersistentMessage message) {
        return message == null ? "" : extractTabGroupTitle(message.attribution);
    }

    private static @Nullable String extractTabGroupTitle(@Nullable MessageAttribution attribution) {
        return attribution == null || attribution.tabGroupMetadata == null
                ? ""
                : attribution.tabGroupMetadata.lastKnownTitle;
    }

    private static @Nullable Token extractTabGroupId(@Nullable MessageAttribution attribution) {
        return attribution == null
                        || attribution.tabGroupMetadata == null
                        || attribution.tabGroupMetadata.localTabGroupId == null
                ? null
                : attribution.tabGroupMetadata.localTabGroupId.tabGroupId;
    }

    /** Returns the collaboration id or null. */
    public static @Nullable String extractCollaborationId(@Nullable InstantMessage message) {
        if (message == null) return null;
        MessageAttribution attribution = getFirstAttribution(message);
        return attribution == null ? null : attribution.collaborationId;
    }

    /** Returns a GroupMember associated with the message, prioritizing affected over triggering. */
    public static @Nullable GroupMember extractMember(@Nullable InstantMessage message) {
        return message == null ? null : extractMember(getFirstAttribution(message));
    }

    /** Returns a GroupMember associated with the message, prioritizing affected over triggering. */
    public static @Nullable GroupMember extractMember(@Nullable PersistentMessage message) {
        return message == null ? null : extractMember(message.attribution);
    }

    private static @Nullable GroupMember extractMember(@Nullable MessageAttribution attribution) {
        if (attribution == null) {
            return null;
        } else if (attribution.affectedUser != null) {
            return attribution.affectedUser;
        } else {
            return attribution.triggeringUser;
        }
    }

    /** Returns the url of the tab or null. */
    public static @Nullable String extractTabUrl(@Nullable InstantMessage message) {
        if (message == null) return null;
        MessageAttribution attribution = getFirstAttribution(message);
        return attribution == null || attribution.tabMetadata == null
                ? null
                : attribution.tabMetadata.lastKnownUrl;
    }

    /** Returns the previous url of the tab or null. */
    public static @Nullable String extractPrevTabUrl(@Nullable InstantMessage message) {
        if (message == null) return null;
        MessageAttribution attribution = getFirstAttribution(message);
        return attribution == null || attribution.tabMetadata == null
                ? null
                : attribution.tabMetadata.previousUrl;
    }

    private static @Nullable String extractSyncTabGroupId(
            @Nullable MessageAttribution attribution) {
        return attribution == null || attribution.tabGroupMetadata == null
                ? null
                : attribution.tabGroupMetadata.syncTabGroupId;
    }

    /** Returns the message id or null. */
    public static @Nullable String extractMessageId(@Nullable InstantMessage message) {
        if (message == null) return null;
        MessageAttribution attribution = getFirstAttribution(message);
        return attribution == null ? null : attribution.id;
    }

    /** Returns the message id or null. */
    public static @Nullable String extractMessageId(@Nullable PersistentMessage message) {
        return message == null || message.attribution == null ? null : message.attribution.id;
    }
}
