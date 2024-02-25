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
     * Called before a message is shown to allow the delegate to do preparation work. Should be
     * called only once before showing.
     * @param callback The callback called after all the preparation work has been done.
     */
    void onRequestShowing(Runnable callback);

    /** Called after all messages are finished hiding. Should be called only once after hiding. */
    void onFinishHiding();

    /** Called when a message animation is about to start. */
    void onAnimationStart();

    /** Called after a message animation has ended. */
    void onAnimationEnd();

    /**
     * Returns whether the browser/UI is ready to show items from the queue.
     * @return Whether everything is ready for showing a new message.
     */
    boolean isReadyForShowing();

    /**
     * Returns whether the delegate is preparing to show a message.
     * @return True if {@link #onRequestShowing(Runnable)} is called but not finished yet.
     */
    boolean isPendingShow();

    /**
     * Returns whether the associated activity has been destroyed.
     * @return True if the lifecycle has been destroyed such that no animation will be resumed.
     */
    boolean isDestroyed();

    /**
     * Returns whether the queue is switching to another scope. This is used to catch some edge
     * cases in which the browser control is not ready while the scope is about to change.
     * @return Whether the queue is switching to another scope.
     */
    boolean isSwitchingScope();
}
