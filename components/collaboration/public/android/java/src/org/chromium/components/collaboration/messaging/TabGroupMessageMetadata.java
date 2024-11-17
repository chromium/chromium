// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import androidx.annotation.Nullable;

import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.Optional;

/**
 * Metadata about a tab within {@link MessageAttribution}. This is a shim layer for the native
 * representation of the object. See //components/collaboration/public/messaging/message.h for
 * specific details.
 */
public class TabGroupMessageMetadata {
    @Nullable public LocalTabGroupId localTabGroupId;
    @Nullable public String syncTabGroupId;
    @Nullable public String lastKnownTitle;
    // Use {@link #hasColor()} to inspect if this field has a value.
    // The Integer should be assumed to be of type {@link TabGroupColorId}.
    public Optional<Integer> lastKnownColor;
}
