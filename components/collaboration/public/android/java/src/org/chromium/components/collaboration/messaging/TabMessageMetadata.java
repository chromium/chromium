// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import androidx.annotation.Nullable;

/**
 * Metadata about a tab group within {@link MessageAttribution}. This is a shim layer for the native
 * representation of the object. See //components/collaboration/public/messaging/message.h for
 * specific details.
 */
public class TabMessageMetadata {
    /** Should match Tab.INVALID_TAB_ID. Cannot have a true dependency. */
    /*package*/ static final int INVALID_TAB_ID = -1;

    public int localTabId = INVALID_TAB_ID;
    @Nullable public String syncTabId;
    @Nullable public String lastKnownUrl;
    @Nullable public String lastKnownTitle;
}
