// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.Queue;

/**
 * A class managing the queue of messages. Its primary role is to decide when to show/hide current
 * message and which message to show next.
 */
class MessageQueueManagerImpl implements MessageQueueManager, UnownedUserData {
    private static final UnownedUserDataKey<MessageQueueManagerImpl> KEY =
            new UnownedUserDataKey<>(MessageQueueManagerImpl.class);

    private final Queue<MessageStateHandler> mMessageQueue = new ArrayDeque<>();
    private final Map<Object, MessageStateHandler> mMessageMap = new HashMap<>();
    @Nullable
    private MessageStateHandler mCurrentDisplayedMessage;

    /**
     * Get the activity's MessageQueueManager from the provided WindowAndroid.
     * @param window The window to get the manager from.
     * @return The activity's MessageQueueManager.
     */
    public static MessageQueueManagerImpl from(WindowAndroid window) {
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    public MessageQueueManagerImpl() {}

    /**
     * Attaches MessageQueueManager to a given window. This window will be used later to retrieve
     * activity's MessageQueueManager.
     * @param window The window to attach to.
     */
    public void attachToWindowAndroid(WindowAndroid window) {
        KEY.attachToHost(window.getUnownedUserDataHost(), this);
    }

    /**
     * Destroys MessageQueueManager, detaching it from the WindowAndroid it was attached to.
     */
    @Override
    public void destroy() {
        KEY.detachFromAllHosts(this);
    }

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
