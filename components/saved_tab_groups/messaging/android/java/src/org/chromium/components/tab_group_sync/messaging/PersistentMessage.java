// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

/**
 * Represents a message from the Tab Group Sharing system, where something of note requires a
 * persistent indicator in the UI layer. This is a shim layer for the native representation of the
 * object. See //components/saved_tab_groups/messaging/message.h for specific details.
 */
public class PersistentMessage {
    public MessageAttribution attribution;
    @UserAction public int action;
    @PersistentNotificationType public int type;
}
