// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * Delegate for message queue to call when a message is about to show and hide. The delegate should
 * do preparation work and then call the given callback after preparation is finished.
 */
public interface MessageQueueDelegate {
    /**
     * Call to do preparation work before showing a message.
     * @param callback The callback called after all the preparation work has been done.
     */
    void prepareToShow(Runnable callback);

    /**
     * Call to do preparation work after hiding a message.
     * @param callback The callback called before all the preparation work has been done.
     */
    void prepareToHide(Runnable callback);
}
