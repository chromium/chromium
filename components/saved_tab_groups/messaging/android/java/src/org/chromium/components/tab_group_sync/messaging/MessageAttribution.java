// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import androidx.annotation.Nullable;

import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

/**
 * Attribution data for an {@link InstantMessage} or {@link PersistentMessage}. This is a shim layer
 * for the native representation of the object. See
 * //components/saved_tab_groups/messaging/message.h for specific details.
 */
public class MessageAttribution {
    @Nullable public LocalTabGroupId localTabGroupId;
    @Nullable public String syncTabGroupId;
    public int localTabId = -1;
    @Nullable public String syncTabId;
    @Nullable public GroupMember affectedUser;
    public GroupMember triggeringUser;
    public String collaborationId;
}
