// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The public interface for Messages. To interact with messages, feature should obtain a reference
 * to MessageDispatcher through MessageDispatcherProvider and call methods of MessageDispatcher.
 */
public interface MessageDispatcher {
    /**
     * Enqueues a message defined by its properties.
     * @param messageProperties The PropertyModel with message's visual properties.
     * @param webContents The webContents the message is associated with.
     * @param scopeType The {@link MessageScopeType} of the message.
     * @param highPriority True if the message should be displayed ASAP.
     */
    void enqueueMessage(
            PropertyModel messageProperties,
            WebContents webContents,
            @MessageScopeType int scopeType,
            boolean highPriority);

    /**
     * Enqueues a message defined by its properties. This message will be of a
     * {@link MessageScopeType#WINDOW} scope. And it will be be associated with WindowAndroid for
     * which this message dispatcher was created. Use {@link
     * MessageDispatcher#enqueueMessage(PropertyModel, WebContents, int, boolean)} to ensure the
     * message is attached to given webContents or the windowAndroid associated with the given
     * webContents.
     *
     * @param messageProperties The PropertyModel with message's visual properties.
     * @param highPriority True if the message should be displayed ASAP.
     */
    void enqueueWindowScopedMessage(PropertyModel messageProperties, boolean highPriority);

    /**
     * Dismisses a message referenced by its PropertyModel. Hides the message if it is currently
     * displayed. Displays the next message in the queue if available.
     * @param messageProperties The PropertyModel of the message to be dismissed.
     * @param dismissReason The reason why the message is being dismissed.
     */
    void dismissMessage(PropertyModel messageProperties, @DismissReason int dismissReason);
}
