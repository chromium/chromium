// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

/**
 * Metadata about a tab within {@link MessageAttribution}. This is a shim layer for the native
 * representation of the object. See //components/collaboration/public/messaging/message.h for
 * specific details.
 */
@NullMarked
public class TabGroupMessageMetadata {
    public @Nullable LocalTabGroupId localTabGroupId;
    public @Nullable String syncTabGroupId;
    public @Nullable String lastKnownTitle;

    // Use {@link #hasColor()} to inspect if this field has a value.
    // The Integer should be assumed to be of type {@link TabGroupColorId}.
    public @Nullable Integer lastKnownColor;
}
