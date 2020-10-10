// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.Queue;

/**
 * A class managing the queue of messages. Its primary role is to decide when to show/hide current
 * message and which message to show next.
 */
class MessageQueueManager {
    private final Queue<MessageStateHandler> mMessageQueue = new ArrayDeque<>();
    private final Map<Object, MessageStateHandler> mMessageMap = new HashMap<>();
    @Nullable
    private MessageStateHandler mCurrentDisplayedMessage;

    public MessageQueueManager() {}

    /**
     * Enqueues a message. Associates the message with its key; the key is used later to dismiss the
     * message. Displays the message if there is no other message shown.
     * @param message The message to enqueue
     * @param key The key to associate with this message.
     */
    public void enqueueMessage(MessageStateHandler message, Object key) {
        if (mMessageMap.containsKey(key)) {
            throw new IllegalStateException("Message with the given key has already been enqueued");
        }
        mMessageMap.put(key, message);
        mMessageQueue.add(message);
        updateCurrentDisplayedMessage();
    }

    /**
     * Dismisses a message specified by its key. Hdes the message if it is currently displayed.
     * Displays the next message in the queue if available.
     * @param key The key associated with the message to dismiss.
     */
    public void dismissMessage(Object key) {
        MessageStateHandler message = mMessageMap.get(key);
        if (message == null) return;
        mMessageMap.remove(key);
        mMessageQueue.remove(message);
        if (mCurrentDisplayedMessage == message) {
            mCurrentDisplayedMessage.hide();
            mCurrentDisplayedMessage = null;
        }
        message.dismiss();
        updateCurrentDisplayedMessage();
    }

    private void updateCurrentDisplayedMessage() {
        if (mCurrentDisplayedMessage != null) return;
        if (mMessageQueue.isEmpty()) return;
        mCurrentDisplayedMessage = mMessageQueue.element();
        mCurrentDisplayedMessage.show();
    }
}
