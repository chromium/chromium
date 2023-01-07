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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Coordinator for toggling animation when message is about to show or hide.
 */
public class MessageAnimationCoordinator {
    private static final String TAG = MessageQueueManager.TAG;
    // Animation start delay for the back message for MessageBannerMediator.ENTER_DURATION_MS amount
    // of time, required to show the front message from Position.INVISIBLE to Position.FRONT.
    private static final int BACK_MESSAGE_START_DELAY_MS = 600;

    /**
     * mCurrentDisplayedMessage refers to the message which is currently visible on the screen
     * including situations in which the message is already dismissed and hide animation is running.
     */
    @Nullable
    private MessageState mCurrentDisplayedMessage;
    @NonNull
    private List<MessageState> mCurrentDisplayedMessages = Arrays.asList(null, null);
    private MessageState mLastShownMessage;
    private MessageQueueDelegate mMessageQueueDelegate;
    private AnimatorSet mAnimatorSet = new AnimatorSet();
    private Animator mFrontAnimator;
    private Animator mBackAnimator;
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

    // TODO(crbug.com/1200974): Compare current shown messages with last shown ones.
    /**
     * cf: Current front message.
     * cb: Current back message.
     * nf: Next front message.
     * nb: Next back message.
     * Null represents no view at that position.
     * 1. If candidates and current displayed messages are internally equal, do nothing.
     * 2. If cf is null, which implies cb is also null, show candidates.
     * 3. If cf is not found in candidates, it must be hidden.
     *    In the meantime, if current back message is displayed, check if it should be hidden or
     *    moved to front.
     * 4. If only back message is changed:
     *    Hide current back message if possible; otherwise, show the candidate.
     * 5. The current front message must be moved back and a new message is moved to front.
     *
     * Note: Assume current displayed messages are [m1, m2]; Then the candidates won't be [m3, m2].
     * If m3 is a higher priority message, then the candidates should be [m3, m1].
     * Otherwise, m1 is usually hidden because of dismissing or inactive scope, the candidates
     * should be [m2, null/m3].
     *
     * [m1, m2] -> [m3, m4] should also be impossible, because message is designed to be dismissed
     * one by one. If both are hiding by queue suspending, it should look like:
     * [m1, m2] -> [null, null] -> [m3, m4]
     *
     * @param candidates The candidates supposed to be displayed next. Not all candidates are
     *                   guaranteed to be displayed after update. The content may be changed to
     *                   reflect the actual change in this update.
     * @param isSuspended Whether the queue is suspended.
     * @param onFinished Runnable triggered after animation is finished.
     */
    public void updateWithStacking(
            @NonNull List<MessageState> candidates, boolean isSuspended, Runnable onFinished) {
        // Wait until the current animation is done, unless we need to hide them immediately.
        if (!isSuspended && mAnimatorSet.isStarted()) {
            return;
        }
        var cf = mCurrentDisplayedMessages.get(0); // Currently front.
        var cb = mCurrentDisplayedMessages.get(1); // Currently back.
        var nf = candidates.get(0); // Next front.
        var nb = candidates.get(1); // Next back.
        mFrontAnimator = mBackAnimator = null;
        boolean animate = !isSuspended;

        // If front message is null, then the back one is definitely null.
        assert !(nf == null && nb != null);
        assert !(cf == null && cb != null);
        if (cf == nf && cb == nb) return;
        if (cf == null) { // Implies that currently back is also null.
            mFrontAnimator = nf.handler.show(Position.INVISIBLE, Position.FRONT);
            if (nb != null) {
                mBackAnimator = nb.handler.show(Position.FRONT, Position.BACK);
                if (mBackAnimator != null) {
                    mBackAnimator.setStartDelay(BACK_MESSAGE_START_DELAY_MS);
                }
            }
        } else if (cf != nf && cf != nb) {
            // Current displayed front message will be hidden.
            mFrontAnimator = cf.handler.hide(Position.FRONT, Position.INVISIBLE, animate);
            if (cb != null) {
                if (cb == nf) { // Visible front will be dismissed and back one is moved to front.
                    mBackAnimator = cb.handler.show(Position.BACK, Position.FRONT);
                    // Show nb in the next round.
                    nb = null;
                    candidates.set(1, null);
                } else { // Both visible front and back messages will be replaced.
                    mBackAnimator = cb.handler.hide(Position.BACK, Position.FRONT, animate);
                    // Hide current displayed two messages and then show other messages
                    // in the next round.
                    nf = nb = null;
                    candidates.set(0, null);
                    candidates.set(1, null);
                }
            }
        } else if (cf == nf) {
            if (cb != null) { // Hide the current back one.
                mBackAnimator = cb.handler.hide(Position.BACK, Position.FRONT, animate);
            } else {
                // If nb is null, it means candidates and current displayed messages are equal.
                assert nb != null;
                mBackAnimator = nb.handler.show(Position.FRONT, Position.BACK);
            }
        } else {
            assert cf == nb;
            if (cb != null) {
                mBackAnimator = cb.handler.hide(Position.BACK, Position.FRONT, animate);
                // [m1, m2] -> [m1, null] -> [m3, m1]
                // In this case, we complete this in 2 steps to avoid manipulating 3 handlers
                // at any single moment.
                candidates.set(0, cf);
                candidates.set(1, null);
            } else { // Moved the current front to back and show a new front view.
                mBackAnimator = cf.handler.show(Position.FRONT, Position.BACK);
                mFrontAnimator = nf.handler.show(Position.INVISIBLE, Position.FRONT);
            }
        }
        if (cf == null) {
            // No message is being displayed now: trigger #onStartShowing.
            mMessageQueueDelegate.onStartShowing(
                    () -> { triggerStackingAnimation(candidates, onFinished); });
        } else if (nf == null) {
            // All messages will be hidden: trigger #onFinishHiding.
            Runnable runnable = () -> {
                mMessageQueueDelegate.onFinishHiding();
                mCurrentDisplayedMessage = mLastShownMessage = null;
                onFinished.run();
            };
            triggerStackingAnimation(candidates, runnable);
        } else {
            triggerStackingAnimation(candidates, onFinished);
        }
    }

    private void triggerStackingAnimation(List<MessageState> candidates, Runnable onFinished) {
        mCurrentDisplayedMessages = new ArrayList<>(candidates);
        Runnable runnable = () -> {
            mAnimatorSet.cancel();
            mAnimatorSet.removeAllListeners();
            mAnimatorSet = new AnimatorSet();
            mAnimatorSet.play(mFrontAnimator);
            mAnimatorSet.play(mBackAnimator);
            mAnimatorSet.addListener(new MessageAnimationListener(onFinished));
            mAnimatorStartCallback.onResult(mAnimatorSet);
        };
        if (candidates.get(0) == null) {
            runnable.run();
        } else {
            mContainer.runAfterInitialMessageLayout(runnable);
        }
    }

    void setMessageQueueDelegate(MessageQueueDelegate delegate) {
        mMessageQueueDelegate = delegate;
    }

    @Nullable
    MessageState getCurrentDisplayedMessage() {
        return mCurrentDisplayedMessage;
    }

    // Return a list of two messages which should be displayed when stacking animation is enabled.
    @NonNull
    List<MessageState> getCurrentDisplayedMessages() {
        return mCurrentDisplayedMessages;
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
}
