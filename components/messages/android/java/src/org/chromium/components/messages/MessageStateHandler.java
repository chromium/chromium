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
     */
    void hide();

    /**
     * Notify that the message is about to be dismissed from the queue.
     */
    void dismiss();
}