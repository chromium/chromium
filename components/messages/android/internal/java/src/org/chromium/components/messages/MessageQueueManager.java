// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;

import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A class managing the queue of messages. Its primary role is to decide when to show/hide current
 * message and which message to show next.
 */
class MessageQueueManager {
    /**
     * mCurrentDisplayedMessage refers to the message which is currently visible on the screen
     * including situations in which the message is already dismissed and hide animation is running.
     */
    @Nullable
    private MessageQueueManager.MessageState mCurrentDisplayedMessage;
    private MessageQueueDelegate mMessageQueueDelegate;
    // TokenHolder tracking whether the queue should be suspended.
    private final TokenHolder mSuppressionTokenHolder =
            new TokenHolder(this::updateCurrentDisplayedMessage);

    /**
     * A {@link Map} collection which contains {@code MessageKey} as the key and the corresponding
     * {@link MessageState} as the value.
     * When the message is dismissed, it is immediately removed from this collection even though the
     * message could still be visible with hide animation running.
     */
    private final Map<Object, MessageState> mMessages = new HashMap<>();
    /**
     * A {@link Map} collection which contains {@code scopeKey} as the key and and a list of
     * {@link MessageState} containing all of the messages associated with this scope instance as
     * the value.
     * When the message is dismissed, it is immediately removed from this collection even though the
     * message could still be visible with hide animation running.
     */
    private final Map<Object, List<MessageState>> mMessageQueues = new HashMap<>();
    /**
     * A {@link Map} collection which contains {@code scopeKey} as the key and a boolean
     * value standing for whether this scope instance is active or not as the value.
     */
    private final Map<Object, Boolean> mScopeStates = new HashMap<>();

    /**
     * Enqueues a message. Associates the message with its key; the key is used later to dismiss the
     * message. Displays the message if there is no other message shown.
     * @param message The message to enqueue
     * @param messageKey The key to associate with this message.
     * @param scopeType The type of scope.
     * @param scopeKey The key of a scope instance.
     */
    public void enqueueMessage(
            MessageStateHandler message, Object messageKey, int scopeType, Object scopeKey) {
        if (mMessages.containsKey(messageKey)) {
            throw new IllegalStateException("Message with the given key has already been enqueued");
        }

        List<MessageState> messageQueue = mMessageQueues.get(scopeKey);
        if (messageQueue == null) {
            messageQueue = new ArrayList<>();
            mMessageQueues.put(scopeKey, messageQueue);
        }

        MessageState messageState = new MessageState(scopeKey, messageKey, message);
        messageQueue.add(messageState);
        mMessages.put(messageKey, messageState);

        updateCurrentDisplayedMessage();
    }

    /**
     * Dismisses a message specified by its key. Hides the message if it is currently displayed.
     * Displays the next message in the queue if available.
     *
     * @param messageKey The key associated with the message to dismiss.
     * @param dismissReason The reason why message is being dismissed.
     */
    public void dismissMessage(Object messageKey, @DismissReason int dismissReason) {
        MessageState messageState = mMessages.get(messageKey);
        if (messageState == null) return;
        MessageStateHandler message = messageState.handler;
        Object scopeKey = messageState.scopeKey;

        mMessages.remove(messageKey);

        // Remove the scope from the map if the messageQueue is empty.
        List<MessageState> messageQueue = mMessageQueues.get(scopeKey);
        messageQueue.remove(messageState);
        if (messageQueue.isEmpty()) {
            mMessageQueues.remove(scopeKey);
        }

        if (mCurrentDisplayedMessage == messageState) {
            mCurrentDisplayedMessage.handler.hide(true, () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage = null;
                message.dismiss(dismissReason);
                updateCurrentDisplayedMessage();
            });
        } else {
            message.dismiss(dismissReason);
        }
    }

    public int suspend() {
        return mSuppressionTokenHolder.acquireToken();
    }

    public void resume(int token) {
        mSuppressionTokenHolder.releaseToken(token);
    }

    public void setDelegate(MessageQueueDelegate delegate) {
        mMessageQueueDelegate = delegate;
    }

    private void updateCurrentDisplayedMessage() {
        updateCurrentDisplayedMessage(true);
    }

    private boolean isQueueSuspended() {
        return mSuppressionTokenHolder.hasTokens();
    }

    // TODO(crbug.com/1163290): Rethink the case where a message show or dismiss animation is
    //      running when we get another scope change signal that should potentially either reverse
    //      the animation (i.e. going from inactive -> active quickly) or jump to the end (i.e.
    //      going from animate transition -> don't animate transition.
    private void updateCurrentDisplayedMessage(boolean animateTransition) {
        if (mCurrentDisplayedMessage == null && !isQueueSuspended()) {
            mCurrentDisplayedMessage = getNextMessage();
            if (mCurrentDisplayedMessage != null) {
                mMessageQueueDelegate.onStartShowing(mCurrentDisplayedMessage.handler::show);
            }
        } else if (mCurrentDisplayedMessage != null) {
            // Scope state may be removed if it has been destroyed.
            boolean isScopeActive = mScopeStates.containsKey(mCurrentDisplayedMessage.scopeKey)
                    && mScopeStates.get(mCurrentDisplayedMessage.scopeKey);
            if (isQueueSuspended() || !isScopeActive) {
                mCurrentDisplayedMessage.handler.hide(
                        !isQueueSuspended() && animateTransition, () -> {
                            mMessageQueueDelegate.onFinishHiding();
                            mCurrentDisplayedMessage = null;
                        });
            }
        }
    }

    // TODO(crbug.com/1163290): Handle the case in which the scope becomes inactive when the
    //         message is already running the animation.
    void onScopeChange(MessageScopeChange change) {
        Object scopeKey = change.scopeInstanceKey;
        if (change.changeType == ChangeType.DESTROY) {
            List<MessageState> messages = mMessageQueues.get(scopeKey);
            mScopeStates.remove(scopeKey);
            if (messages != null) {
                while (!messages.isEmpty()) {
                    // message will be removed from messages list.
                    dismissMessage(messages.get(0).key, DismissReason.SCOPE_DESTROYED);
                }
            }
        } else if (change.changeType == ChangeType.INACTIVE) {
            mScopeStates.put(scopeKey, false);
            updateCurrentDisplayedMessage(change.animateTransition);
        } else if (change.changeType == ChangeType.ACTIVE) {
            mScopeStates.put(scopeKey, true);
            updateCurrentDisplayedMessage();
        }
    }

    void dismissAllMessages(@DismissReason int dismissReason) {
        for (MessageState m : mMessages.values()) {
            MessageStateHandler handler = m.handler;
            if (m == mCurrentDisplayedMessage) {
                handler.hide(false, () -> {
                    mMessageQueueDelegate.onFinishHiding();
                    handler.dismiss(dismissReason);
                    mCurrentDisplayedMessage = null;
                });
            } else {
                handler.dismiss(dismissReason);
            }
        }
        mMessages.clear();
        mMessageQueues.clear();
        mScopeStates.clear();
    }

    /**
     * Iterate the queues of each scope to get the next messages. If multiple messages meet the
     * requirements, which can show in the given scope, then the message queued earliest will be
     * returned.
     */
    private MessageState getNextMessage() {
        MessageState nextMessage = null;
        for (List<MessageState> queue : mMessageQueues.values()) {
            if (queue.isEmpty()) continue;
            MessageState candidate = queue.get(0);
            Boolean isActive = mScopeStates.get(candidate.scopeKey);
            if (isActive == null || !isActive) continue;
            if (nextMessage == null || candidate.id < nextMessage.id) nextMessage = candidate;
        }
        return nextMessage;
    }

    private static class MessageState {
        private static int sIdNext;

        // TODO(crbug.com/1168693): add priority if necessary.
        public final int id;
        public final Object scopeKey;
        public final Object key;
        public final MessageStateHandler handler;

        MessageState(Object scopeKey, Object key, MessageStateHandler handler) {
            this.scopeKey = scopeKey;
            this.key = key;
            this.handler = handler;
            id = sIdNext++;
        }
    }
}
