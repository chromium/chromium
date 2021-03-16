// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class implements public MessageDispatcher interface, delegating the actual work to
 * MessageQueueManager.
 */
public class MessageDispatcherImpl implements ManagedMessageDispatcher {
    private final MessageQueueManager mMessageQueueManager = new MessageQueueManager();
    private final MessageContainer mMessageContainer;
    private final Supplier<Integer> mMessageMaxTranslationSupplier;
    private final Supplier<Long> mAutodismissDurationMs;
    private final Callback<Animator> mAnimatorStartCallback;

    /**
     * Build a new message dispatcher
     * @param messageContainer A container view for displaying message banners.
     * @param messageMaxTranslation A {@link Supplier} that supplies the maximum translation Y value
     *         the message banner can have as a result of the animations or the gestures.
     * @param autodismissDurationMs A {@link Supplier} providing autodismiss duration for message
     *         banner.
     * @param animatorStartCallback The {@link Callback} that will be used by the message to
     *         delegate starting the animations to the {@link WindowAndroid}.
     */
    public MessageDispatcherImpl(MessageContainer messageContainer,
            Supplier<Integer> messageMaxTranslation, Supplier<Long> autodismissDurationMs,
            Callback<Animator> animatorStartCallback) {
        mMessageContainer = messageContainer;
        mMessageMaxTranslationSupplier = messageMaxTranslation;
        mAnimatorStartCallback = animatorStartCallback;
        mAutodismissDurationMs = autodismissDurationMs;
    }

    @Override
    public void enqueueMessage(PropertyModel messageProperties, WebContents webContents,
            @MessageScopeType int scopeType) {
        MessageStateHandler messageStateHandler = new SingleActionMessage(mMessageContainer,
                messageProperties, this::dismissMessage, mMessageMaxTranslationSupplier,
                mAutodismissDurationMs, mAnimatorStartCallback);
        ScopeKey scopeKey = new ScopeKey(scopeType, webContents);
        mMessageQueueManager.enqueueMessage(
                messageStateHandler, messageProperties, scopeType, scopeKey);
    }

    @Override
    public void dismissMessage(PropertyModel messageProperties, @DismissReason int dismissReason) {
        mMessageQueueManager.dismissMessage(messageProperties, dismissReason);
    }

    @Override
    public void dismissAllMessages(@DismissReason int dismissReason) {
        mMessageQueueManager.dismissAllMessages(dismissReason);
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
