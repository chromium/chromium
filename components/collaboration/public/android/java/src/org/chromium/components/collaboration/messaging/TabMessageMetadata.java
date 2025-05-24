// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Metadata about a tab group within {@link MessageAttribution}. This is a shim layer for the native
 * representation of the object. See //components/collaboration/public/messaging/message.h for
 * specific details.
 */
@NullMarked
public class TabMessageMetadata {
    /** Should match Tab.INVALID_TAB_ID. Cannot have a true dependency. */
    public static final int INVALID_TAB_ID = -1;

    public int localTabId = INVALID_TAB_ID;
    public @Nullable String syncTabId;
    public @Nullable String lastKnownUrl;
    public @Nullable String lastKnownTitle;
    public @Nullable String previousUrl;
}
