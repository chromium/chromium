// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/**
 * The central controller for versioning messages. Determines which versioning messages should be
 * shown based on whether the current chrome client is able to support shared tab groups. The UI
 * layer should query this class before showing any versioning messages.
 */
@NullMarked
public interface VersioningMessageController {
    /**
     * Invoke this method to query if the given message UI should be shown. This will internally
     * wait for TabGroupSyncService initialization.
     *
     * @param messageType The {@link MessageType} to query.
     * @param callback The callback to be invoked with the result.
     */
    void shouldShowMessageUiAsync(@MessageType int messageType, Callback<Boolean> callback);

    /**
     * Invoke this after a MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE or
     * MessageType.VERSION_UPDATED_MESSAGE has been displayed.
     *
     * @param messageType The {@link MessageType} that was shown.
     */
    void onMessageUiShown(@MessageType int messageType);

    /**
     * Invoke this after a user dismisses a MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE.
     *
     * @param messageType The {@link MessageType} that was dismissed.
     */
    void onMessageUiDismissed(@MessageType int messageType);
}
