// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class implements public MessageDispatcher interface, delegating the actual work to
 * MessageQueueManager.
 */
public class MessageDispatcherImpl implements ManagedMessageDispatcher {
    private final MessageQueueManager mMessageQueueManager = new MessageQueueManager();
    private final MessageContainer mMessageContainer;

    /**
     * Build a new message dispatcher
     * @param messageContainer A container view for displaying message banners.
     */
    public MessageDispatcherImpl(MessageContainer messageContainer) {
        mMessageContainer = messageContainer;
    }

    @Override
    public void enqueueMessage(PropertyModel messageProperties) {
        MessageStateHandler messageStateHandler =
                new SingleActionMessage(mMessageContainer, messageProperties);
        mMessageQueueManager.enqueueMessage(messageStateHandler, messageProperties);
    }

    @Override
    public void dismissMessage(PropertyModel messageProperties) {
        mMessageQueueManager.dismissMessage(messageProperties);
    }
}
