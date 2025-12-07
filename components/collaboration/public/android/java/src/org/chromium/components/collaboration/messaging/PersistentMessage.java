// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents a message from the Tab Group Sharing system, where something of note requires a
 * persistent indicator in the UI layer. This is a shim layer for the native representation of the
 * object. See //components/collaboration/public/messaging/message.h for specific details.
 * TODO: Add a proper constructor to avoid @SuppressWarnings("NullAway.Init")
 */
@NullMarked
public class PersistentMessage {
    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public MessageAttribution attribution;

    @CollaborationEvent public int collaborationEvent;
    @PersistentNotificationType public int type;
}
