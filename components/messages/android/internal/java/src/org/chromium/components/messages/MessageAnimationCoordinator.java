// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.animation.AnimatorSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageStateHandler.Position;

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
    @Nullable
    private MessageState mCurrentDisplayedMessage;
    @Nullable
    private List<MessageState> mCurrentDisplayedMessages;
    private MessageState mLastShownMessage;
    private MessageQueueDelegate mMessageQueueDelegate;
    private AnimatorSet mAnimatorSet = new AnimatorSet();
    private final MessageContainer mContainer;
    private final Callback<Animator> mAnimatorStartCallback;

    public MessageAnimationCoordinator(
            MessageContainer messageContainer, Callback<Animator> animatorStartCallback) {
        mContainer = messageContainer;
        mAnimatorStartCallback = animatorStartCallback;
    }

    public void updateWithoutStacking(
            @Nullable MessageState candidate, boolean suspended, Runnable onFinished) {
        if (mCurrentDisplayedMessage == candidate) return;
        if (!suspended && mAnimatorSet.isStarted()) {
            return;
        }
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

                final var animator =
                        mCurrentDisplayedMessage.handler.show(Position.INVISIBLE, Position.FRONT);

                // Wait until the message and the container are measured before showing the message.
                // This is required in case the animation set-up requires the height of the
                // container, e.g. showing messages without the top controls visible.
                mContainer.runAfterInitialMessageLayout(() -> {
                    mAnimatorSet.cancel();
                    mAnimatorSet.removeAllListeners();

                    mAnimatorSet = new AnimatorSet();
                    mAnimatorSet.play(animator);
                    mAnimatorStartCallback.onResult(mAnimatorSet);
                });
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
            mAnimatorSet.cancel();
            mAnimatorSet.removeAllListeners();

            Animator animator = mCurrentDisplayedMessage.handler.hide(
                    Position.FRONT, Position.INVISIBLE, !suspended);
            if (animator == null) {
                runnable.run();
            } else {
                mAnimatorSet = new AnimatorSet();
                mAnimatorSet.play(animator);
                mAnimatorSet.addListener(new MessageAnimationListener(runnable));
                mAnimatorStartCallback.onResult(mAnimatorSet);
            }
        }
    }

    // TODO(crbug.com/1200974): Add support for stacking animation.
    public void updateWithStacking(
            @NonNull List<MessageState> candidates, boolean isSuspended, Runnable onFinished) {}

    void setMessageQueueDelegate(MessageQueueDelegate delegate) {
        mMessageQueueDelegate = delegate;
    }

    @Nullable
    MessageState getCurrentDisplayedMessage() {
        return mCurrentDisplayedMessage;
    }

    class MessageAnimationListener extends CancelAwareAnimatorListener {
        private final Runnable mOnFinished;

        public MessageAnimationListener(Runnable onFinished) {
            mOnFinished = onFinished;
        }

        @Override
        public void onEnd(Animator animator) {
            super.onEnd(animator);
            mOnFinished.run();
        }
    }

    // Return a list of two messages which should be displayed when stacking animation is enabled.
    List<MessageState> getCurrentDisplayedMessages() {
        return mCurrentDisplayedMessages;
    }
}
