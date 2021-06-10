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

    /** Returns MessageIdentifier of the current message. */
    @MessageIdentifier
    int getMessageIdentifier();
}
