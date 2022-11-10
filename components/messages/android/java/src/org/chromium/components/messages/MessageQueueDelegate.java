// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * Delegate for message queue to call when a message is about to show and hide. The delegate should
 * do preparation work and then call the given callback after preparation is finished.
 */
public interface MessageQueueDelegate {
    /**
     * Called before a message is shown to allow the delegate to do preparation work.
     * @param callback The callback called after all the preparation work has been done.
     */
    void onStartShowing(Runnable callback);

    /**
     * Called after a message is finished hiding.
     */
    void onFinishHiding();

    /**
     * Called when a message animation is about to start.
     */
    void onAnimationStart();

    /**
     * Called after a message animation has ended.
     */
    void onAnimationEnd();
}
