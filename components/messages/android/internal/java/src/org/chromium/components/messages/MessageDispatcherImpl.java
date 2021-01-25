// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * This class implements public MessageDispatcher interface, delegating the actual work to
 * MessageQueueManager.
 */
public class MessageDispatcherImpl implements ManagedMessageDispatcher {
    private final MessageQueueManager mMessageQueueManager = new MessageQueueManager();
    private final MessageContainer mMessageContainer;
    private final Supplier<Integer> mMessageMaxTranslationSupplier;
    private final AccessibilityUtil mAccessibilityUtil;
    private final Callback<Animator> mAnimatorStartCallback;

    /**
     * Build a new message dispatcher
     * @param messageContainer A container view for displaying message banners.
     * @param messageMaxTranslation A {@link Supplier} that supplies the maximum translation Y value
     *         the message banner can have as a result of the animations or the gestures.
     * @param accessibilityUtil A util to expose information related to system accessibility state.
     * @param animatorStartCallback The {@link Callback} that will be used by the message to
     *         delegate starting the animations to the {@link WindowAndroid}.
     */
    public MessageDispatcherImpl(MessageContainer messageContainer,
            Supplier<Integer> messageMaxTranslation, AccessibilityUtil accessibilityUtil,
            Callback<Animator> animatorStartCallback) {
        mMessageContainer = messageContainer;
        mMessageMaxTranslationSupplier = messageMaxTranslation;
        mAccessibilityUtil = accessibilityUtil;
        mAnimatorStartCallback = animatorStartCallback;
    }

    @Override
    public void enqueueMessage(PropertyModel messageProperties) {
        MessageStateHandler messageStateHandler =
                new SingleActionMessage(mMessageContainer, messageProperties, this::dismissMessage,
                        mMessageMaxTranslationSupplier, mAccessibilityUtil, mAnimatorStartCallback);
        mMessageQueueManager.enqueueMessage(messageStateHandler, messageProperties);
    }

    @Override
    public void dismissMessage(PropertyModel messageProperties) {
        mMessageQueueManager.dismissMessage(messageProperties);
    }

    @Override
    public void dismissAllMessages() {
        mMessageQueueManager.dismissAllMessages();
    }

    @Override
    public int suspend() {
        return mMessageQueueManager.suspend();
    }

    @Override
    public void resume(int token) {
        mMessageQueueManager.resume(token);
    }

    @Override
    public void setDelegate(MessageQueueDelegate delegate) {
        mMessageQueueManager.setDelegate(delegate);
    }
}
