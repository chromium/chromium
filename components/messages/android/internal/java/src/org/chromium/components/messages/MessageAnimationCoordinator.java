// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.components.messages.MessageQueueManager.MessageState;

import java.util.List;

/**
 * Coordinator for toggling animation when message is about to show or hide.
 */
public class MessageAnimationCoordinator {
    private static final String TAG = MessageQueueManager.TAG;

    /**
     * mCurrentDisplayedMessage refers to the message which is currently visible on the screen
     * including situations in which the message is already dismissed and hide animation is running.
     */
    private MessageState mCurrentDisplayedMessage;
    private MessageState mLastShownMessage;
    private MessageQueueDelegate mMessageQueueDelegate;

    public void updateWithoutStacking(
            @Nullable MessageState candidate, boolean suspended, Runnable onFinished) {
        if (mCurrentDisplayedMessage == candidate) return;
        if (mCurrentDisplayedMessage == null) {
            mCurrentDisplayedMessage = candidate;
            mMessageQueueDelegate.onStartShowing(() -> {
                if (mCurrentDisplayedMessage == null) {
                    return;
                }
                Log.w(TAG,
                        "MessageStateHandler#shouldShow for message with ID %s and key %s in "
                                + "MessageQueueManager#updateCurrentDisplayedMessage "
                                + "returned %s.",
                        candidate.handler.getMessageIdentifier(), candidate.messageKey,
                        candidate.handler.shouldShow());
                mCurrentDisplayedMessage.handler.show();
                mLastShownMessage = mCurrentDisplayedMessage;
                onFinished.run();
            });
        } else {
            Runnable runnable = () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage = mLastShownMessage = null;
                onFinished.run();
            };
            if (mLastShownMessage != mCurrentDisplayedMessage) {
                runnable.run();
                return;
            }
            mCurrentDisplayedMessage.handler.hide(!suspended, runnable);
        }
    }

    // TODO(crbug.com/1200974): Add support for stacking animation.
    public void updateWithStacking(
            @NonNull List<MessageState> candidates, boolean isSuspended, Runnable onFinished) {}

    void setMessageQueueDelegate(MessageQueueDelegate delegate) {
        mMessageQueueDelegate = delegate;
    }

    MessageState getCurrentDisplayedMessage() {
        return mCurrentDisplayedMessage;
    }
}
