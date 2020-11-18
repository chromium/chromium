// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.util.TokenHolder;

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
    private MessageQueueDelegate mMessageQueueDelegate;
    private final TokenHolder mTokenHolder;

    public MessageQueueManager() {
        mTokenHolder = new TokenHolder(this::updateCurrentDisplayedMessage);
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
        Runnable onAnimationFinished = () -> {
            message.dismiss();
            updateCurrentDisplayedMessage();
        };
        if (mCurrentDisplayedMessage == message) {
            mCurrentDisplayedMessage.hide(true, () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage = null;
                onAnimationFinished.run();
            });
        } else {
            onAnimationFinished.run();
        }
    }

    public int suspend() {
        return mTokenHolder.acquireToken();
    }

    public void resume(int token) {
        mTokenHolder.releaseToken(token);
    }

    public void setDelegate(MessageQueueDelegate delegate) {
        mMessageQueueDelegate = delegate;
    }

    private void updateCurrentDisplayedMessage() {
        // TODO(crbug.com/1123947): may only call delegate when message system goes from
        //                            no shown message to showing first message or the opposite.
        if (mMessageQueue.isEmpty()) {
            assert mCurrentDisplayedMessage
                    == null : "No message should be displayed when the queue is empty.";
            return;
        }
        if (mCurrentDisplayedMessage == null && !mTokenHolder.hasTokens()) {
            mCurrentDisplayedMessage = mMessageQueue.element();
            mMessageQueueDelegate.onStartShowing(mCurrentDisplayedMessage::show);
        } else if (mCurrentDisplayedMessage != null && mTokenHolder.hasTokens()) {
            mCurrentDisplayedMessage.hide(false, () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage = null;
            });
        }
    }

    void dismissAllMessages() {
        if (mCurrentDisplayedMessage != null) {
            mMessageQueue.remove(mCurrentDisplayedMessage);
            mCurrentDisplayedMessage.hide(false, () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage.dismiss();
                mCurrentDisplayedMessage = null;
            });
        }
        for (MessageStateHandler h : mMessageQueue) {
            h.dismiss();
        }
        mMessageMap.clear();
        mMessageQueue.clear();
    }

    @VisibleForTesting
    Queue<MessageStateHandler> getMessageQueueForTesting() {
        return mMessageQueue;
    }

    @VisibleForTesting
    Map<Object, MessageStateHandler> getMessageMapForTesting() {
        return mMessageMap;
    }
}
