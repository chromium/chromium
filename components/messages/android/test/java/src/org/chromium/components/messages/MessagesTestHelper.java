// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** A helper class providing utility methods that are intended to be used in tests using messages. */
@JNINamespace("messages")
public class MessagesTestHelper {
    private TestMessageDispatcherWrapper mWrapper;
    private ManagedMessageDispatcher mOriginalDispatcher;
    private WeakReference<WindowAndroid> mWindowRef;
    private final long mNativePointer;

    private MessagesTestHelper(long nativePointer) {
        mNativePointer = nativePointer;
    }

    @CalledByNative
    private static MessagesTestHelper init(long nativePointer) {
        return new MessagesTestHelper(nativePointer);
    }

    /**
     * Set a test-only message dispatcher for the given window. Used for C++ test that does not have
     * a real message dispatcher during tests.
     */
    @CalledByNative
    private void attachTestMessageDispatcherForTesting(WindowAndroid windowAndroid) {
        var original = (ManagedMessageDispatcher) MessageDispatcherProvider.from(windowAndroid);
        if (original != null) {
            mOriginalDispatcher = original;
            mWindowRef = new WeakReference<>(windowAndroid);
        }
        mWrapper = new TestMessageDispatcherWrapper(original);
        MessagesFactory.attachMessageDispatcher(windowAndroid, mWrapper);
        mWrapper.addObserver(() -> MessagesTestHelperJni.get().onMessageEnqueued(mNativePointer));

        ResettersForTesting.register(this::resetMessageDispatcherForTesting);
    }

    /** Remove the test-only dispatcher from the window and reset with the original dispatcher. */
    @CalledByNative
    private void resetMessageDispatcherForTesting() {
        if (mWrapper != null) {
            MessagesFactory.detachMessageDispatcher(mWrapper);
        }
        if (mOriginalDispatcher != null && mWindowRef != null && mWindowRef.get() != null) {
            MessagesFactory.attachMessageDispatcher(mWindowRef.get(), mOriginalDispatcher);
            mWindowRef = null;
            mOriginalDispatcher = null;
        }
    }

    private static MessageDispatcherImpl getDispatcherImplFromWindow(WindowAndroid windowAndroid) {
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(windowAndroid);
        if (dispatcher instanceof TestMessageDispatcherWrapper) {
            return (MessageDispatcherImpl)
                    ((TestMessageDispatcherWrapper) dispatcher).getWrappedDispatcher();
        }
        return (MessageDispatcherImpl) dispatcher;
    }

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
        MessageDispatcherImpl messageDispatcherImpl = getDispatcherImplFromWindow(windowAndroid);
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
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(windowAndroid);
        MessageDispatcherImpl messageDispatcherImpl;
        if (dispatcher instanceof TestMessageDispatcherWrapper) {
            messageDispatcherImpl =
                    (MessageDispatcherImpl)
                            ((TestMessageDispatcherWrapper) dispatcher).getWrappedDispatcher();
        } else {
            messageDispatcherImpl = (MessageDispatcherImpl) dispatcher;
        }
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

    @NativeMethods
    interface Natives {
        void onMessageEnqueued(long nativeMessagesTestHelper);
    }
}
