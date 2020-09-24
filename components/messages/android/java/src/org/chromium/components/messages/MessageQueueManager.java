// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * The public interface for managing the queue of messages. The embedder should retain reference to
 * MessageQueueManager and destroy it when the Activity gets destroyed.
 */
public interface MessageQueueManager {
    /**
     * Destroys MessageQueueManager, detaching it from the WindowAndroid it was attached to.
     */
    void destroy();
}
