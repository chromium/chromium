// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class implements public MessageDispatcher interface, delegating the actual work to
 * MessageQueueManager.
 */
public class MessageDispatcherImpl implements ManagedMessageDispatcher {
    private final MessageQueueManager mMessageQueueManager;
    private final MessageContainer mMessageContainer;
    private final Supplier<Integer> mMessageTopOffset;
    private final Supplier<Integer> mMessageMaxTranslationSupplier;
    private final MessageAutodismissDurationProvider mAutodismissDurationProvider;
    private final SwipeAnimationHandler mSwipeAnimationHandler;
    private final WindowAndroid mWindowAndroid;

    /**
     * Build a new message dispatcher
     *
     * @param messageContainer A container view for displaying message banners.
     * @param messageTopOffset A {@link Supplier} that supplies the top offset between message's top
     * side and app's top edge.
     * @param messageMaxTranslation A {@link Supplier} that supplies the maximum translation Y value
     * the message banner can have as a result of the animations or the gestures.
     * @param autodismissDurationProvider A {@link MessageAutodismissDurationProvider} providing
     * autodismiss duration for message banner.
     * @param animatorStartCallback The {@link Callback} that will be used by the message to
     * delegate starting the animations to the {@link WindowAndroid}.
     * @param windowAndroid The current window Android.
     */
    public MessageDispatcherImpl(
            MessageContainer messageContainer,
            Supplier<Integer> messageTopOffset,
            Supplier<Integer> messageMaxTranslation,
            MessageAutodismissDurationProvider autodismissDurationProvider,
            Callback<Animator> animatorStartCallback,
            WindowAndroid windowAndroid) {
        this(
                messageContainer,
                messageTopOffset,
                messageMaxTranslation,
                autodismissDurationProvider,
                windowAndroid,
                new MessageQueueManager(
                        new MessageAnimationCoordinator(messageContainer, animatorStartCallback)));
    }

    @VisibleForTesting
    MessageDispatcherImpl(
            MessageContainer messageContainer,
            Supplier<Integer> messageTopOffset,
            Supplier<Integer> messageMaxTranslation,
            MessageAutodismissDurationProvider autodismissDurationProvider,
            WindowAndroid windowAndroid,
            MessageQueueManager messageQueueManager) {
        mMessageContainer = messageContainer;
        mMessageTopOffset = messageTopOffset;
        mMessageMaxTranslationSupplier = messageMaxTranslation;
        mAutodismissDurationProvider = autodismissDurationProvider;
        mWindowAndroid = windowAndroid;
        mMessageQueueManager = messageQueueManager;
        mSwipeAnimationHandler = messageQueueManager.getAnimationCoordinator();
    }

    /**
     * Enqueue navigation or webContents scoped message.
     * @param messageProperties The PropertyModel with message's visual properties.
     * @param webContents The webContents the message is associated with.
     * @param scopeType The {@link MessageScopeType} of the message.
     * @param highPriority True if the message should be displayed ASAP.
     */
    @Override
    public void enqueueMessage(
            PropertyModel messageProperties,
            WebContents webContents,
            @MessageScopeType int scopeType,
            boolean highPriority) {
        MessageStateHandler messageStateHandler =
                new SingleActionMessage(
                        mMessageContainer,
                        messageProperties,
                        this::dismissMessage,
                        mMessageMaxTranslationSupplier,
                        mMessageTopOffset,
                        mAutodismissDurationProvider,
                        mSwipeAnimationHandler);
        ScopeKey scopeKey;
        assert scopeType != MessageScopeType.WINDOW
                : "Use #enqueueWindowScopedMessage to enqueue a window-scoped message.";
        scopeKey = new ScopeKey(scopeType, webContents);
        mMessageQueueManager.enqueueMessage(
                messageStateHandler, messageProperties, scopeKey, highPriority);
    }

    @Override
    public void enqueueWindowScopedMessage(PropertyModel messageProperties, boolean highPriority) {
        MessageStateHandler messageStateHandler =
                new SingleActionMessage(
                        mMessageContainer,
                        messageProperties,
                        this::dismissMessage,
                        mMessageMaxTranslationSupplier,
                        mMessageTopOffset,
                        mAutodismissDurationProvider,
                        mSwipeAnimationHandler);
        ScopeKey scopeKey = new ScopeKey(mWindowAndroid);
        mMessageQueueManager.enqueueMessage(
                messageStateHandler, messageProperties, scopeKey, highPriority);
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

    MessageQueueManager getMessageQueueManagerForTesting() {
        return mMessageQueueManager;
    }
}
