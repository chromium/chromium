// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import androidx.annotation.Nullable;

import org.chromium.components.data_sharing.GroupMember;

/**
 * Attribution data for an {@link InstantMessage} or {@link PersistentMessage}. This is a shim layer
 * for the native representation of the object. See
 * //components/saved_tab_groups/messaging/message.h for specific details.
 */
public class MessageAttribution {
    @Nullable public TabGroupMessageMetadata tabGroupMetadata;
    @Nullable public TabMessageMetadata tabMetadata;
    @Nullable public GroupMember affectedUser;
    public GroupMember triggeringUser;
    public String collaborationId;
}
