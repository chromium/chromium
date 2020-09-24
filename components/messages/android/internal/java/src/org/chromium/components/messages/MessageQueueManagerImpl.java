// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class managing the queue of messages. Its primary role is to decide when to show/hide current
 * message and which message to show next.
 */
class MessageQueueManagerImpl implements MessageQueueManager, UnownedUserData {
    private static final UnownedUserDataKey<MessageQueueManagerImpl> KEY =
            new UnownedUserDataKey<>(MessageQueueManagerImpl.class);

    /**
     * Get the activity's MessageQueueManager from the provided WindowAndroid.
     * @param window The window to get the manager from.
     * @return The activity's MessageQueueManager.
     */
    public static MessageQueueManagerImpl from(WindowAndroid window) {
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    public MessageQueueManagerImpl() {}

    /**
     * Attaches MessageQueueManager to a given window. This window will be used later to retrieve
     * activity's MessageQueueManager.
     * @param window The window to attach to.
     */
    public void attachToWindowAndroid(WindowAndroid window) {
        KEY.attachToHost(window.getUnownedUserDataHost(), this);
    }

    /**
     * Destroys MessageQueueManager, detaching it from the WindowAndroid it was attached to.
     */
    @Override
    public void destroy() {
        KEY.detachFromAllHosts(this);
    }
}
