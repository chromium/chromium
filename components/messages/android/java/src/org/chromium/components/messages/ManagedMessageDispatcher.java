// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/** An interface for the MessageDispatcher owning object. */
public interface ManagedMessageDispatcher
        extends MessageDispatcher, MessageDispatcherProvider.Unowned {
    /**
     * Suspend the dispatcher to prevent Messages from being displayed.
     * @return A token required to resume the dispatcher.
     */
    int suspend();

    /**
     * Resume the dispatcher to allow to show new messages.
     * @param token A token returned by {@link #suspend()};
     */
    void resume(int token);

    /**
     * Set a {@link MessageQueueDelegate} to do show/hide related preparation work.
     * @param delegate The {@link MessageQueueDelegate}.
     */
    void setDelegate(MessageQueueDelegate delegate);

    /**
     * Dismiss all the enqueued messages. The currently being displayed message will be
     * hidden at once without animations.
     * @param dismissReason The reason why messages are dismissed.
     */
    void dismissAllMessages(@DismissReason int dismissReason);
}
