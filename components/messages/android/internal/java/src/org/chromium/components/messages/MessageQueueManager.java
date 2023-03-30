// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * A class managing the queue of messages. Its primary role is to decide when to show/hide current
 * message and which message to show next.
 */
class MessageQueueManager implements ScopeChangeController.Delegate {
    static final String TAG = "MessageQueueManager";

    // TokenHolder tracking whether the queue should be suspended.
    private final TokenHolder mSuppressionTokenHolder =
            new TokenHolder(this::onSuspendedStateChange);
    private final MessageAnimationCoordinator mAnimationCoordinator;

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
    private final Map<ScopeKey, Boolean> mScopeStates = new HashMap<>();

    private ScopeChangeController mScopeChangeController = new ScopeChangeController(this);

    public MessageQueueManager(MessageAnimationCoordinator animationCoordinator) {
        mAnimationCoordinator = animationCoordinator;
    }

    /**
     * Enqueues a message. Associates the message with its key; the key is used later to dismiss the
     * message. Displays the message if there is no other message shown.
     * @param message The message to enqueue
     * @param messageKey The key to associate with this message.
     * @param scopeKey The key of a scope instance.
     * @param highPriority True if the message should be displayed ASAP.
     */
    public void enqueueMessage(MessageStateHandler message, Object messageKey, ScopeKey scopeKey,
            boolean highPriority) {
        if (mMessages.containsKey(messageKey)) {
            throw new IllegalStateException("Message with the given key has already been enqueued");
        }

        List<MessageState> messageQueue = mMessageQueues.get(scopeKey);
        if (messageQueue == null) {
            mMessageQueues.put(scopeKey, messageQueue = new ArrayList<>());
            mScopeChangeController.firstMessageEnqueued(scopeKey);
        }

        MessageState messageState = new MessageState(scopeKey, messageKey, message, highPriority);
        messageQueue.add(messageState);
        mMessages.put(messageKey, messageState);

        MessagesMetrics.recordMessageEnqueued(message.getMessageIdentifier());
        if (MessageFeatureList.isStackAnimationEnabled()) {
            updateCurrentDisplayedWithStacking();
        } else {
            MessageState candidate = updateCurrentDisplayedWithoutStacking();
            if (candidate != null) {
                Log.w(TAG, "Currently displaying message with ID %s and key %s.",
                        candidate.handler.getMessageIdentifier(), candidate.messageKey);
            }

            if (candidate == messageState) {
                MessagesMetrics.recordMessageEnqueuedVisible(message.getMessageIdentifier());
            } else {
                @MessageIdentifier
                int visibleMessageId = MessageIdentifier.INVALID_MESSAGE;
                if (candidate != null) visibleMessageId = candidate.handler.getMessageIdentifier();
                MessagesMetrics.recordMessageEnqueuedHidden(
                        message.getMessageIdentifier(), visibleMessageId);
            }
        }
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
        mMessages.remove(messageKey);
        dismissMessageInternal(messageState, dismissReason);
    }

    /**
     * This method updates related structure and dismiss the queue, but does not remove the
     * message state from the queue.
     */
    private void dismissMessageInternal(
            @NonNull MessageState messageState, @DismissReason int dismissReason) {
        MessageStateHandler message = messageState.handler;
        ScopeKey scopeKey = messageState.scopeKey;

        // Remove the scope from the map if the messageQueue is empty.
        List<MessageState> messageQueue = mMessageQueues.get(scopeKey);
        messageQueue.remove(messageState);
        Log.w(TAG, "Removed message with ID %s and key %s from queue because of reason %s.",
                message.getMessageIdentifier(), messageState.messageKey, dismissReason);
        if (messageQueue.isEmpty()) {
            mMessageQueues.remove(scopeKey);
            mScopeChangeController.lastMessageDismissed(scopeKey);
        }

        message.dismiss(dismissReason);
        updateCurrentDisplayedMessages();
        MessagesMetrics.recordDismissReason(message.getMessageIdentifier(), dismissReason);
    }

    public int suspend() {
        return mSuppressionTokenHolder.acquireToken();
    }

    public void resume(int token) {
        mSuppressionTokenHolder.releaseToken(token);
    }

    public void setDelegate(MessageQueueDelegate delegate) {
        mAnimationCoordinator.setMessageQueueDelegate(delegate);
    }

    // TODO(crbug.com/1163290): Handle the case in which the scope becomes inactive when the
    //         message is already running the animation.
    @Override
    public void onScopeChange(MessageScopeChange change) {
        ScopeKey scopeKey = change.scopeInstanceKey;
        if (change.changeType == ChangeType.DESTROY) {
            List<MessageState> messages = mMessageQueues.get(scopeKey);
            mScopeStates.remove(scopeKey);
            if (messages != null) {
                while (!messages.isEmpty()) {
                    // message will be removed from messages list.
                    dismissMessage(messages.get(0).messageKey, DismissReason.SCOPE_DESTROYED);
                }
            }
        } else if (change.changeType == ChangeType.INACTIVE) {
            mScopeStates.put(scopeKey, false);
            updateCurrentDisplayedMessages();
        } else if (change.changeType == ChangeType.ACTIVE) {
            mScopeStates.put(scopeKey, true);
            updateCurrentDisplayedMessages();
        }
    }

    private void onSuspendedStateChange() {
        updateCurrentDisplayedMessages();
    }

    private boolean isQueueSuspended() {
        return mSuppressionTokenHolder.hasTokens();
    }

    /**
     * Update current displayed message(s). Stacking animation is triggered if it's enabled.
     */
    private void updateCurrentDisplayedMessages() {
        if (MessageFeatureList.isStackAnimationEnabled()) {
            updateCurrentDisplayedWithStacking();
        } else {
            updateCurrentDisplayedWithoutStacking();
        }
    }

    /**
     * Update current displayed messages with stacking.
     * @return The candidates supposed to be displayed.
     */
    private List<MessageState> updateCurrentDisplayedWithStacking() {
        var candidates = getNextMessages();
        mAnimationCoordinator.updateWithStacking(
                candidates, isQueueSuspended(), this::updateCurrentDisplayedWithStacking);
        return candidates;
    }

    // TODO(crbug.com/1163290): Rethink the case where a message show or dismiss animation is
    //      running when we get another scope change signal that should potentially either reverse
    //      the animation (i.e. going from inactive -> active quickly) or jump to the end (i.e.
    //      going from animate transition -> don't animate transition.

    /**
     * Update current displayed message without stacking.
     * @return The candidate supposed to be displayed.
     */
    private MessageState updateCurrentDisplayedWithoutStacking() {
        var candidate = getNextMessage();
        mAnimationCoordinator.updateWithoutStacking(
                candidate, isQueueSuspended(), this::updateCurrentDisplayedWithoutStacking);
        return candidate;
    }

    void dismissAllMessages(@DismissReason int dismissReason) {
        for (MessageState m : mMessages.values()) {
            dismissMessageInternal(m, dismissReason);
        }
        mMessages.clear();
    }

    MessageAnimationCoordinator getAnimationCoordinator() {
        return mAnimationCoordinator;
    }

    void setScopeChangeControllerForTesting(ScopeChangeController controllerForTesting) {
        mScopeChangeController = controllerForTesting;
    }

    Map<Object, MessageState> getMessagesForTesting() {
        return mMessages;
    }

    /**
     * Iterate the queues of each scope to get the next message. If multiple messages meet the
     * requirements, which can show in the given scope, then the message queued earliest will be
     * returned.
     */
    @VisibleForTesting
    MessageState getNextMessage() {
        var nextMessages = getNextMessages();
        assert nextMessages.size() == 2;
        return nextMessages.get(0);
    }

    /**
     * Return the next two messages which should be displayed. The first element stands for the
     * front message while the other one stands for the back message. Null represents no view should
     * be displayed at that position.
     */
    @VisibleForTesting
    List<MessageState> getNextMessages() {
        if (isQueueSuspended()) {
            return Arrays.asList(null, null);
        }
        MessageState a = null;
        MessageState b = null;
        for (var queue : mMessageQueues.values()) {
            if (queue.isEmpty()) continue;
            Boolean isActive = mScopeStates.get(queue.get(0).scopeKey);
            if (!Objects.equals(isActive, true)) continue;
            for (var candidate : queue) {
                boolean shouldShow = candidate.handler.shouldShow();
                Log.w(TAG,
                        "MessageStateHandler#shouldShow for message with ID %s and key %s in "
                                + "MessageQueueManager#getNextMessage returned %s.",
                        candidate.handler.getMessageIdentifier(), candidate.messageKey, shouldShow);
                if (!shouldShow) continue;
                if (isLowerPriority(a, candidate)) {
                    b = a;
                    a = candidate;
                } else if (isLowerPriority(b, candidate)) {
                    b = candidate;
                }
            }
        }
        return Arrays.asList(a, b);
    }

    // Return true if |a| is lower priority than |b|.
    private boolean isLowerPriority(@Nullable MessageState a, @NonNull MessageState b) {
        if (a == null) return true;
        if (!a.highPriority && b.highPriority) return true;
        return a.id > b.id;
    }

    static class MessageState {
        private static int sIdNext;

        public final int id;
        public final ScopeKey scopeKey;
        public final Object messageKey;
        public final MessageStateHandler handler;
        public final boolean highPriority;

        MessageState(ScopeKey scopeKey, Object messageKey, MessageStateHandler handler,
                boolean highPriority) {
            this.scopeKey = scopeKey;
            this.messageKey = messageKey;
            this.handler = handler;
            this.highPriority = highPriority;
            id = sIdNext++;
        }
    }
}
