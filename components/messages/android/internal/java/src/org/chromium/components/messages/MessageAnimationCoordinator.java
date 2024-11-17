// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.messages.MessagesMetrics.recordStackingAnimationType;
import static org.chromium.components.messages.MessagesMetrics.recordThreeStackedScenario;

import android.animation.Animator;
import android.animation.AnimatorSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.components.messages.MessagesMetrics.StackingAnimationAction;
import org.chromium.components.messages.MessagesMetrics.StackingAnimationType;
import org.chromium.components.messages.MessagesMetrics.ThreeStackedScenario;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Coordinator for toggling animation when message is about to show or hide. */
public class MessageAnimationCoordinator implements SwipeAnimationHandler {
    private static final String TAG = MessageQueueManager.TAG;
    // Animation start delay for the back message for MessageBannerMediator.ENTER_DURATION_MS amount
    // of time, required to show the front message from Position.INVISIBLE to Position.FRONT.
    private static final int BACK_MESSAGE_START_DELAY_MS = 600;

    /**
     * mCurrentDisplayedMessage refers to the message which is currently visible on the screen
     * including situations in which the message is already dismissed and hide animation is running.
     */
    @Nullable private MessageState mCurrentDisplayedMessage;

    @NonNull private List<MessageState> mCurrentDisplayedMessages = Arrays.asList(null, null);
    private MessageQueueDelegate mMessageQueueDelegate;
    private AnimatorSet mAnimatorSet = new AnimatorSet();
    private Animator mFrontAnimator;
    private Animator mBackAnimator;
    private final MessageContainer mContainer;
    private final Callback<Animator> mAnimatorStartCallback;
    private final boolean mAreExtraHistogramsEnabled;

    public MessageAnimationCoordinator(
            MessageContainer messageContainer, Callback<Animator> animatorStartCallback) {
        mContainer = messageContainer;
        mAnimatorStartCallback = animatorStartCallback;
        mAreExtraHistogramsEnabled = MessageFeatureList.areExtraHistogramsEnabled();
    }

    // TODO(crbug.com/40762119): Compare current shown messages with last shown ones.
    /**
     * cf: Current front message. cb: Current back message. nf: Next front message. nb: Next back
     * message. Null represents no view at that position. 1. If candidates and current displayed
     * messages are internally equal, do nothing. 2. If cf is null, which implies cb is also null,
     * show candidates. 3. If cf is not found in candidates, it must be hidden. In the meantime, if
     * current back message is displayed, check if it should be hidden or moved to front. 4. If only
     * back message is changed: Hide current back message if possible; otherwise, show the
     * candidate. 5. The current front message must be moved back and a new message is moved to
     * front.
     *
     * <p>Note: Assume current displayed messages are [m1, m2]; Then the candidates won't be [m3,
     * m2]. If m3 is a higher priority message, then the candidates should be [m3, m1]. Otherwise,
     * m1 is usually hidden because of dismissing or inactive scope, the candidates should be [m2,
     * null/m3].
     *
     * <p>[m1, m2] -> [m3, m4] should also be impossible, because message is designed to be
     * dismissed one by one. If both are hiding by queue suspending, it should look like: [m1, m2]
     * -> [null, null] -> [m3, m4]
     *
     * @param candidates The candidates supposed to be displayed next. Not all candidates are
     *     guaranteed to be displayed after update. The content may be changed to reflect the actual
     *     change in this update.
     * @param isSuspended Whether the queue is suspended.
     * @param onFinished Runnable triggered after animation is finished.
     */
    public void updateWithStacking(
            @NonNull List<MessageState> candidates, boolean isSuspended, Runnable onFinished) {
        if (mMessageQueueDelegate.isDestroyed()) return;
        // Wait until the current animation is done, unless we need to hide them immediately.
        if (mAnimatorSet.isStarted()) {
            if (isSuspended) {
                // crbug.com/1405389: Force animation to end in order to trigger callbacks.
                mAnimatorSet.end();
                onFinished.run();
            }
            return;
        }

        var currentFront = mCurrentDisplayedMessages.get(0); // Currently front.
        var currentBack = mCurrentDisplayedMessages.get(1); // Currently back.
        var nextFront = candidates.get(0); // Next front.
        var nextBack = candidates.get(1); // Next back.

        // If front message is null, then the back one is definitely null.
        assert !(nextFront == null && nextBack != null);
        assert !(currentFront == null && currentBack != null);
        assert !isSuspended || nextFront == null : "when suspending, all messages should be hidden";
        if (currentFront == nextFront && currentBack == nextBack) {
            if (currentFront == null && mMessageQueueDelegate.isReadyForShowing()) {
                mMessageQueueDelegate.onFinishHiding();
            }
            return;
        }

        if (mAreExtraHistogramsEnabled && currentFront != nextFront && nextFront != null) {
            MessagesMetrics.recordRequestToFullyShow(nextFront.handler.getMessageIdentifier());
        }

        if (!isSuspended && !mMessageQueueDelegate.isReadyForShowing()) {
            // Make sure everything is ready for showing a message, unless messages are about to
            // be removed immediately. By "showing", it does mean not just triggering a showing
            // animation, but also holding a message view on screen.
            // https://crbug.com/1408627: when showing a second message, it is possible the first
            // message is still waiting for message queue delegate to be ready.
            // Only request to show a message if not requested yet.
            if (!mMessageQueueDelegate.isPendingShow()) {
                mMessageQueueDelegate.onRequestShowing(onFinished);
            }
            if (mAreExtraHistogramsEnabled && currentFront != nextFront && nextFront != null) {
                MessagesMetrics.recordBlockedByBrowserControl(
                        nextFront.handler.getMessageIdentifier());
            }
            return;
        }

        // Similar to above scenario, second message is about trigger another animation while first
        // message is still waiting its animation to be triggered. Early return to avoid cancelling
        // that animation accidentally. Second message will be added after its animation is done.
        if (mContainer.isIsInitializingLayout()) {
            if (mAreExtraHistogramsEnabled && currentFront != nextFront && nextFront != null) {
                MessagesMetrics.recordBlockedByContainerInitializing(
                        nextFront.handler.getMessageIdentifier());
            }
            return;
        }

        // If both animators will be modified, modify FrontAnimator first, because the back message
        // relies on the first message in order to adjust its size.
        mFrontAnimator = mBackAnimator = null;
        boolean animate = !isSuspended;

        if (currentFront == null) { // Implies that currently back is also null.
            recordAnimationAction(StackingAnimationAction.INSERT_AT_FRONT, nextFront);
            mFrontAnimator = nextFront.handler.show(Position.INVISIBLE, Position.FRONT);
            if (nextBack != null) {
                recordAnimationAction(StackingAnimationAction.INSERT_AT_BACK, nextBack);
                recordStackingAnimationType(StackingAnimationType.SHOW_ALL);
                mBackAnimator = nextBack.handler.show(Position.FRONT, Position.BACK);
                if (mBackAnimator != null) {
                    mBackAnimator.setStartDelay(BACK_MESSAGE_START_DELAY_MS);
                }
            } else {
                recordStackingAnimationType(StackingAnimationType.SHOW_FRONT_ONLY);
            }
        } else if (currentFront != nextFront && currentFront != nextBack) {
            // Current displayed front message will be hidden.
            recordAnimationAction(StackingAnimationAction.REMOVE_FRONT, currentFront);
            mFrontAnimator = currentFront.handler.hide(Position.FRONT, Position.INVISIBLE, animate);
            if (currentBack != null) {
                if (currentBack == nextFront) { // Visible front will be dismissed and back one is
                    // moved to front.
                    recordAnimationAction(StackingAnimationAction.PUSH_TO_FRONT, currentBack);
                    recordStackingAnimationType(StackingAnimationType.REMOVE_FRONT_AND_SHOW_BACK);
                    mBackAnimator = currentBack.handler.show(Position.BACK, Position.FRONT);
                    if (nextBack != null) {
                        recordThreeStackedScenario(ThreeStackedScenario.IN_SEQUENCE);
                    }
                    // Show nb in the next round.
                    nextBack = null;
                    candidates.set(1, null);
                } else { // Both visible front and back messages will be replaced.
                    recordAnimationAction(StackingAnimationAction.REMOVE_BACK, currentBack);
                    recordStackingAnimationType(StackingAnimationType.REMOVE_ALL);
                    mBackAnimator =
                            currentBack.handler.hide(Position.BACK, Position.FRONT, animate);
                    // Hide current displayed two messages and then show other messages
                    // in the next round.
                    nextFront = nextBack = null;
                    candidates.set(0, null);
                    candidates.set(1, null);
                }
            } else {
                // TODO(crbug.com/40877229): simplify this into one step.
                // Split the transition: [m1, null] -> [m2, null] into two steps:
                // [m1, null] -> [null, null] -> [m2, null]
                nextFront = nextBack = null;
                candidates.set(0, null);
                candidates.set(1, null);
                recordStackingAnimationType(StackingAnimationType.REMOVE_FRONT_ONLY);
            }
        } else if (currentFront == nextFront) {
            if (currentBack != null) { // Hide the current back one.
                recordAnimationAction(StackingAnimationAction.REMOVE_BACK, currentBack);
                recordStackingAnimationType(StackingAnimationType.REMOVE_BACK_ONLY);
                mBackAnimator = currentBack.handler.hide(Position.BACK, Position.FRONT, animate);
                candidates.set(1, null); // Show next back in next round if non-null.
            } else {
                recordAnimationAction(StackingAnimationAction.INSERT_AT_BACK, nextBack);
                recordStackingAnimationType(StackingAnimationType.SHOW_BACK_ONLY);
                // If nb is null, it means candidates and current displayed messages are equal.
                assert nextBack != null;
                mBackAnimator = nextBack.handler.show(Position.FRONT, Position.BACK);
            }
        } else {
            assert currentFront == nextBack;
            if (currentBack != null) {
                recordAnimationAction(StackingAnimationAction.REMOVE_BACK, currentBack);
                recordStackingAnimationType(StackingAnimationType.REMOVE_BACK_ONLY);
                recordThreeStackedScenario(ThreeStackedScenario.HIGH_PRIORITY);
                mBackAnimator = currentBack.handler.hide(Position.BACK, Position.FRONT, animate);
                // [m1, m2] -> [m1, null] -> [m3, m1]
                // In this case, we complete this in 2 steps to avoid manipulating 3 handlers
                // at any single moment.
                candidates.set(0, currentFront);
                candidates.set(1, null);
            } else { // Moved the current front to back and show a new front view.
                recordAnimationAction(StackingAnimationAction.PUSH_TO_BACK, currentFront);
                recordAnimationAction(StackingAnimationAction.INSERT_AT_FRONT, nextFront);
                recordStackingAnimationType(StackingAnimationType.INSERT_AT_FRONT);
                mFrontAnimator = nextFront.handler.show(Position.INVISIBLE, Position.FRONT);
                mBackAnimator = currentFront.handler.show(Position.FRONT, Position.BACK);
            }
        }

        if (candidates.get(0) != null && candidates.get(1) != null) {
            MessagesMetrics.recordStackingHiding(candidates.get(0).handler.getMessageIdentifier());
            MessagesMetrics.recordStackingHidden(candidates.get(1).handler.getMessageIdentifier());
        }

        if (nextFront == null) {
            // All messages will be hidden: trigger #onFinishHiding.
            Runnable runnable =
                    () -> {
                        mMessageQueueDelegate.onFinishHiding();
                        mCurrentDisplayedMessages = new ArrayList<>(candidates);
                        onFinished.run();
                    };
            triggerStackingAnimation(candidates, runnable, mFrontAnimator, mBackAnimator);
        } else {
            assert mMessageQueueDelegate.isReadyForShowing();
            mCurrentDisplayedMessages = new ArrayList<>(candidates);
            triggerStackingAnimation(candidates, onFinished, mFrontAnimator, mBackAnimator);
        }
    }

    private void triggerStackingAnimation(
            List<MessageState> candidates,
            Runnable onFinished,
            Animator frontAnimator,
            Animator backAnimator) {
        Runnable runnable =
                () -> {
                    // While the runnable is waiting to be triggered, hiding animation might be
                    // triggered: while the hiding animation is running, declare this runnable as
                    // obsolete so that it won't cancel the hiding animation.
                    if (isAnimatorExpired(frontAnimator, backAnimator)) {
                        return;
                    }
                    mAnimatorSet.cancel();
                    mAnimatorSet.removeAllListeners();
                    mAnimatorSet = new AnimatorSet();
                    mAnimatorSet.play(frontAnimator);
                    mAnimatorSet.play(backAnimator);
                    mAnimatorSet.addListener(
                            new MessageAnimationListener(
                                    () -> {
                                        mMessageQueueDelegate.onAnimationEnd();
                                        onFinished.run();
                                    }));
                    mMessageQueueDelegate.onAnimationStart();
                    mAnimatorStartCallback.onResult(mAnimatorSet);
                };
        if (candidates.get(0) == null) {
            runnable.run();
        } else {
            boolean initialized = mContainer.runAfterInitialMessageLayout(runnable);
            if (mAreExtraHistogramsEnabled && !initialized) {
                MessagesMetrics.recordBlockedByContainerNotInitialized(
                        candidates.get(0).handler.getMessageIdentifier());
            }
        }
    }

    private boolean isAnimatorExpired(Animator frontAnimator, Animator backAnimator) {
        return mFrontAnimator != frontAnimator || mBackAnimator != backAnimator;
    }

    @Override
    public void onSwipeStart() {
        // Message shouldn't consume swipe for now because animation is running, e.g.:
        // the front message should not be swiped when back message is running showing animation.
        assert isSwipeEnabled();
        mMessageQueueDelegate.onAnimationStart();
    }

    @Override
    public boolean isSwipeEnabled() {
        return !mAnimatorSet.isStarted();
    }

    @Override
    public void onSwipeEnd(@Nullable Animator animator) {
        if (animator == null) {
            mMessageQueueDelegate.onAnimationEnd();
            return;
        }
        animator.addListener(new MessageAnimationListener(mMessageQueueDelegate::onAnimationEnd));
        mAnimatorStartCallback.onResult(animator);
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

    private void recordAnimationAction(
            @StackingAnimationAction int action, @NonNull MessageState messageState) {
        MessagesMetrics.recordStackingAnimationAction(
                action, messageState.handler.getMessageIdentifier());
    }

    static class MessageAnimationListener extends CancelAwareAnimatorListener {
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

    AnimatorSet getAnimatorSetForTesting() {
        return mAnimatorSet;
    }
}
