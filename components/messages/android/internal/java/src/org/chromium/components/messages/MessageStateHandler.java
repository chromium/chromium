// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * Interface of a handler to show, hide or dismiss the message.
 */
public interface MessageStateHandler {
    /**
     * Signals that the message needs to show its UI.
     */
    void show();

    /**
     * Signals that the message needs to hide its UI.
     * @param animate Whether animation should be run or not.
     * @param hiddenCallback Called when message has finished hiding. This will run no matter
     *                       whether animation is skipped or not.
     */
    void hide(boolean animate, Runnable hiddenCallback);

    /**
     * Notify that the message is about to be dismissed from the queue.
     * @param dismissReason The reason why the message is being dismissed.
     */
    void dismiss(@DismissReason int dismissReason);

    /**
     * Determines if the message should be shown after it is enqueued. This can be used in features
     * that require an enqueued message to be shown or not depending on some prerequisites. It is
     * the responsibility of the feature code to dismiss the message if it is not to be shown
     * permanently.
     * @return true if an enqueued message should be shown, false otherwise.
     */
    default boolean shouldShow() {
        return true;
    }

    /** Returns MessageIdentifier of the current message. */
    @MessageIdentifier
    int getMessageIdentifier();
}
