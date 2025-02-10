// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.components.data_sharing.GroupMember;

/** Provides functions to safely read fields out of messages, performing null checks. */
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
    public static @Nullable Token extractTabGroupId(@Nullable InstantMessage message) {
        return message == null ? null : extractTabGroupId(message.attribution);
    }

    /** Returns the sync id of the group, or null. */
    public static @Nullable String extractSyncTabGroupId(@Nullable InstantMessage message) {
        return message == null ? null : extractSyncTabGroupId(message.attribution);
    }

    /** Returns the sync id of the group, or null. */
    public static @Nullable String extractSyncTabGroupId(@Nullable PersistentMessage message) {
        return message == null ? null : extractSyncTabGroupId(message.attribution);
    }

    /** Returns the given name or the empty string. */
    public static String extractGivenName(@Nullable InstantMessage message) {
        GroupMember member = extractMember(message);
        return member == null ? "" : member.givenName;
    }

    /** Returns the tab title or the empty string. */
    public static String extractTabTitle(@Nullable InstantMessage message) {
        return message == null
                        || message.attribution == null
                        || message.attribution.tabMetadata == null
                ? ""
                : message.attribution.tabMetadata.lastKnownTitle;
    }

    /** Returns the tab group title or the empty string. */
    public static String extractTabGroupTitle(@Nullable InstantMessage message) {
        return message == null ? "" : extractTabGroupTitle(message.attribution);
    }

    /** Returns the tab group title or the empty string. */
    public static String extractTabGroupTitle(@Nullable PersistentMessage message) {
        return message == null ? "" : extractTabGroupTitle(message.attribution);
    }

    private static String extractTabGroupTitle(@Nullable MessageAttribution attribution) {
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
        return message == null || message.attribution == null
                ? null
                : message.attribution.collaborationId;
    }

    /** Returns a GroupMember associated with the message, prioritizing affected over triggering. */
    public static GroupMember extractMember(@Nullable InstantMessage message) {
        return message == null ? null : extractMember(message.attribution);
    }

    /** Returns a GroupMember associated with the message, prioritizing affected over triggering. */
    public static GroupMember extractMember(@Nullable PersistentMessage message) {
        return message == null ? null : extractMember(message.attribution);
    }

    private static GroupMember extractMember(@Nullable MessageAttribution attribution) {
        if (attribution == null) {
            return null;
        } else if (attribution.affectedUser != null) {
            return attribution.affectedUser;
        } else {
            return attribution.triggeringUser;
        }
    }

    /** Returns the url of the tab or null. */
    public static String extractTabUrl(@Nullable InstantMessage message) {
        return message == null
                        || message.attribution == null
                        || message.attribution.tabMetadata == null
                ? null
                : message.attribution.tabMetadata.lastKnownUrl;
    }

    private static @Nullable String extractSyncTabGroupId(
            @Nullable MessageAttribution attribution) {
        return attribution == null || attribution.tabGroupMetadata == null
                ? null
                : attribution.tabGroupMetadata.syncTabGroupId;
    }

    /** Returns the message id or null. */
    public static @Nullable String extractMessageId(@Nullable InstantMessage message) {
        return message == null || message.attribution == null ? null : message.attribution.id;
    }

    /** Returns the message id or null. */
    public static @Nullable String extractMessageId(@Nullable PersistentMessage message) {
        return message == null || message.attribution == null ? null : message.attribution.id;
    }
}
