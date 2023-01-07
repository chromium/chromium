// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A helper class providing utility methods that are intended to be used in tests using messages.
 */
@JNINamespace("messages")
public class MessagesTestHelper {
    /**
     * Get currently enqueued messages of a specific type.
     * @param messageDispatcher The {@link MessageDispatcher} for displaying messages.
     * @param messageIdentifier The identifier of the message.
     * @return A list of {@link MessageStateHandler}s of currently enqueued messages of a specific
     *         type.
     */
    public static List<MessageStateHandler> getEnqueuedMessages(
            MessageDispatcher messageDispatcher, @MessageIdentifier int messageIdentifier) {
        assert messageDispatcher != null;
        MessageDispatcherImpl messageDispatcherImpl = (MessageDispatcherImpl) messageDispatcher;
        List<MessageStateHandler> messages = new ArrayList<>();
        MessageQueueManager queueManager =
                messageDispatcherImpl.getMessageQueueManagerForTesting(); // IN-TEST
        List<MessageState> messageStates =
                new ArrayList<>(queueManager.getMessagesForTesting().values()); // IN-TEST
        for (MessageState messageState : messageStates) {
            if (messageState.handler.getMessageIdentifier() == messageIdentifier) {
                messages.add(messageState.handler);
            }
        }
        return messages;
    }

    /**
     * Get the number of enqueued messages.
     * @param windowAndroid The current window.
     * @return The number of enqueued messages.
     */
    @CalledByNative
    public static int getMessageCount(WindowAndroid windowAndroid) {
        MessageDispatcherImpl messageDispatcherImpl =
                (MessageDispatcherImpl) (MessageDispatcherProvider.from(windowAndroid));
        MessageQueueManager queueManager =
                messageDispatcherImpl.getMessageQueueManagerForTesting(); // IN-TEST
        List<MessageState> messageStates =
                new ArrayList<>(queueManager.getMessagesForTesting().values()); // IN-TEST
        return messageStates.size();
    }

    /**
     * Get the identifier of the enqueued message at a specified index with respect to the message
     * queue.
     * @param windowAndroid The current window.
     * @param index The index of the enqueued message.
     * @return The identifier of the enqueued message.
     */
    @CalledByNative
    public static int getMessageIdentifier(WindowAndroid windowAndroid, int index) {
        MessageDispatcherImpl messageDispatcherImpl =
                (MessageDispatcherImpl) (MessageDispatcherProvider.from(windowAndroid));
        MessageQueueManager queueManager =
                messageDispatcherImpl.getMessageQueueManagerForTesting(); // IN-TEST
        List<MessageState> messageStates =
                new ArrayList<>(queueManager.getMessagesForTesting().values()); // IN-TEST
        return messageStates.get(index).handler.getMessageIdentifier();
    }

    /**
     * Get the property model of a message.
     * @param messageStateHandler The {@link MessageStateHandler} of an active message.
     * @return The {@link PropertyModel} of a message if applicable. Currently supported
     *         implementations include {@link SingleActionMessage}.
     */
    public static PropertyModel getCurrentMessage(MessageStateHandler messageStateHandler) {
        assert messageStateHandler != null;
        assert messageStateHandler instanceof SingleActionMessage;
        return ((SingleActionMessage) messageStateHandler).getModelForTesting();
    }
}
