// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import androidx.annotation.Nullable;

/**
 * Metadata about a tab group within {@link MessageAttribution}. This is a shim layer for the native
 * representation of the object. See //components/saved_tab_groups/messaging/message.h for specific
 * details.
 */
public class TabMessageMetadata {
    public int localTabId = -1;
    @Nullable public String syncTabId;
    @Nullable public String lastKnownUrl;
    @Nullable public String lastKnownTitle;
}
