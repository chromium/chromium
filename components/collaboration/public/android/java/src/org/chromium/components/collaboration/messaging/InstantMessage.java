// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents a message from the Tab Group Sharing system, where something of note requires an
 * instant message to the UI layer. This is a shim layer for the native representation of the
 * object. See //components/collaboration/public/messaging/message.h for specific details. TODO: Add
 * a proper constructor to avoid @SuppressWarnings("NullAway.Init")
 */
@NullMarked
public class InstantMessage {
    @CollaborationEvent public int collaborationEvent;
    @InstantNotificationLevel public int level;
    @InstantNotificationType public int type;

    @SuppressWarnings("NullAway.Init") // This is set to a non-null value immediately after init
    public String localizedMessage;

    // The list of message attributions for the messages that it represents.
    // For single message case, the size is 1. For aggregated message case, it
    // will be greater than 1.`
    public List<MessageAttribution> attributions = new ArrayList<>();

    /**
     * @return Whether this message is a non-aggregated message.
     */
    public boolean isSingleMessage() {
        return attributions.size() == 1;
    }
}
