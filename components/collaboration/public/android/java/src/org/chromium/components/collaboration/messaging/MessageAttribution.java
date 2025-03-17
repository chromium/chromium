// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupMember;

/**
 * Attribution data for an {@link InstantMessage} or {@link PersistentMessage}. This is a shim layer
 * for the native representation of the object. See
 * //components/collaboration/public/messaging/message.h for specific details.
 * TODO: Add a proper constructor to avoid @SuppressWarnings("NullAway.Init")
 */
@NullMarked
public class MessageAttribution {
    public @Nullable String id;

    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String collaborationId;

    public @Nullable TabGroupMessageMetadata tabGroupMetadata;
    public @Nullable TabMessageMetadata tabMetadata;
    public @Nullable GroupMember affectedUser;
    public boolean affectedUserIsSelf;
    public @Nullable GroupMember triggeringUser;
    public boolean triggeringUserIsSelf;
}
