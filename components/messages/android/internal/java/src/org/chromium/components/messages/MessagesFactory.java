// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.ui.base.WindowAndroid;

/** A factory for constructing different Messages related objects. */
public class MessagesFactory {
    /**
     * Creates an instance of MessageQueueManager and attaches it to WindowAndroid.
     * @param windowAndroid The WindowAndroid to attach MessageQueueManager to.
     * @return The constructed MessageQueueManager.
     */
    public static MessageQueueManager createMessageQueueManager(WindowAndroid windowAndroid) {
        MessageQueueManagerImpl messageQueueManager = new MessageQueueManagerImpl();
        messageQueueManager.attachToWindowAndroid(windowAndroid);
        return messageQueueManager;
    }
}