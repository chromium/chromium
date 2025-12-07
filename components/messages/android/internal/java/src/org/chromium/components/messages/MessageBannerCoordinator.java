// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.content.res.Resources;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityEventCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.listmenu.ListMenuHost.PopupMenuShownListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.RunnableTimer;

import java.util.function.Supplier;

/** Coordinator responsible for creating a message banner. */
@NullMarked
class MessageBannerCoordinator {
    private final MessageBannerMediator mMediator;
    private final MessageBannerView mView;
    private final View mParentView;
    private final PropertyModel mModel;
    private final RunnableTimer mTimer;
    private final Supplier<Long> mAutodismissDurationMs;
    private final Runnable mOnTimeUp;

    /**
     * Constructs the message banner.
     *
     * @param view The inflated {@link MessageBannerView}.
     * @param model The model for the message banner.
     * @param maxTranslationSupplier A {@link Supplier} that supplies the maximum translation Y
     *     value the message banner can have as a result of the animations or the gestures.
     * @param topOffsetSupplier A {@link Supplier} that supplies the message's top offset.
     * @param resources The {@link Resources}.
     * @param parentView The parent {@link View}.
     * @param messageDismissed The {@link Runnable} that will run if and when the user dismisses the
     *     message.
     * @param swipeAnimationHandler The handler that will be used to delegate starting the
     *     animations to {@link WindowAndroid} so the message is not clipped as a result of some
     *     Android SurfaceView optimization.
     * @param autodismissDurationMs A {@link Supplier} providing autodismiss duration for message
     *     banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    MessageBannerCoordinator(
            MessageBannerView view,
            PropertyModel model,
            Supplier<Integer> maxTranslationSupplier,
            Supplier<Integer> topOffsetSupplier,
            Resources resources,
            View parentView,
            Runnable messageDismissed,
            SwipeAnimationHandler swipeAnimationHandler,
            Supplier<Long> autodismissDurationMs,
            Runnable onTimeUp) {
        mView = view;
        mParentView = parentView;
        mModel = model;
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator =
                new MessageBannerMediator(
                        model,
                        topOffsetSupplier,
                        maxTranslationSupplier,
                        resources,
                        messageDismissed,
                        swipeAnimationHandler);
        mAutodismissDurationMs = autodismissDurationMs;
        mTimer = new RunnableTimer();
        mOnTimeUp = onTimeUp;
        view.setSwipeHandler(mMediator);
        view.setPopupMenuShownListener(
                createPopupMenuShownListener(mTimer, mAutodismissDurationMs.get(), mOnTimeUp));
    }

    /**
     * Creates a {@link PopupMenuShownListener} to handle secondary button popup menu events on the
     * message banner.
     *
     * @param timer The {@link RunnableTimer} controlling the message banner dismiss duration.
     * @param duration The auto dismiss duration for the message banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    @VisibleForTesting
    PopupMenuShownListener createPopupMenuShownListener(
            RunnableTimer timer, long duration, Runnable onTimeUp) {
        return new PopupMenuShownListener() {
            @Override
            public void onPopupMenuShown() {
                timer.cancelTimer();
            }

            @Override
            public void onPopupMenuDismissed() {
                timer.startTimer(duration, onTimeUp);
            }
        };
    }

    /**
     * Shows the message banner.
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param messageDimensSupplier Supplier of dimensions of the message next to current one.
     * @return The animator which shows the message view.
     */
    Animator show(
            @Position int fromIndex,
            @Position int toIndex,
            Supplier<MessageDimens> messageDimensSupplier) {
        mView.dismissSecondaryMenuIfShown();
        int verticalOffset = 0;
        if (toIndex == Position.BACK) {
            MessageDimens prevMessageDimens = messageDimensSupplier.get();
            int height = mView.getHeight();
            if (!mView.isLaidOut()) {
                int maxWidth = prevMessageDimens.getWidth();
                int wSpec = View.MeasureSpec.makeMeasureSpec(maxWidth, View.MeasureSpec.AT_MOST);
                int hSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
                mView.measure(wSpec, hSpec);
                height = mView.getMeasuredHeight();
            }
            if (height < prevMessageDimens.getHeight()) {
                verticalOffset = prevMessageDimens.getHeight() - height;
            } else if (height > prevMessageDimens.getHeight()) {
                mView.resizeForStackingAnimation(
                        prevMessageDimens.getTitleHeight(),
                        prevMessageDimens.getDescriptionHeight(),
                        prevMessageDimens.getPrimaryButtonLineCount());
            }
        } else if (fromIndex == Position.BACK && toIndex == Position.FRONT) {
            mView.resetForStackingAnimation();
        }
        return mMediator.show(
                fromIndex,
                toIndex,
                verticalOffset,
                () -> {
                    if (toIndex != Position.FRONT) {
                        setOnTouchRunnable(null);
                        setOnTitleChanged(null);
                        mTimer.cancelTimer();
                        // Make it unable to be focused if it is not in the front.
                        mView.enableA11y(false);
                        updateAccessibilityPane(toIndex);
                    } else {
                        mView.enableA11y(true);
                        setOnTouchRunnable(mTimer::resetTimer);
                        updateAccessibilityPane(toIndex);
                        setOnTitleChanged(
                                () -> {
                                    mTimer.resetTimer();
                                    updateAccessibilityPane(toIndex);
                                });
                        mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
                    }
                });
    }

    /**
     * Hides the message banner.
     *
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     * @return The animator which hides the message view.
     */
    @Nullable Animator hide(
            @Position int fromIndex,
            @Position int toIndex,
            boolean animate,
            Runnable messageHidden) {
        mView.dismissSecondaryMenuIfShown();
        mTimer.cancelTimer();
        // Skip animation if animation has been globally disabled.
        // Otherwise, child animator's listener's onEnd will be called immediately after onStart,
        // even before parent animatorSet's listener's onStart.
        var isAnimationDisabled = AccessibilityState.prefersReducedMotion();
        return mMediator.hide(
                fromIndex,
                toIndex,
                animate && !isAnimationDisabled,
                () -> {
                    setOnTouchRunnable(null);
                    setOnTitleChanged(null);
                    sendPaneChangeAccessibilityEvent(/* isShowing= */ false);
                    messageHidden.run();
                });
    }

    void cancelTimer() {
        mTimer.cancelTimer();
    }

    void startTimer() {
        mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
    }

    void setOnTouchRunnable(@Nullable Runnable runnable) {
        mMediator.setOnTouchRunnable(runnable);
    }

    private void updateAccessibilityPane(int toIndex) {
        String msg = "";
        if (toIndex == Position.FRONT) {
            msg =
                    mModel.get(MessageBannerProperties.TITLE)
                            + " "
                            + mView.getResources().getString(R.string.message_screen_position);
        } else {
            msg = mView.getResources().getString(R.string.message_new_actions_available);
        }
        ViewCompat.setAccessibilityPaneTitle(mParentView, msg);
        sendPaneChangeAccessibilityEvent(/* isShowing= */ true);
    }

    /**
     * Sends accessibility events for pane appearance/disappearance when the message is shown/hidden
     * respectively. This should ideally move accessibility focus automatically to/out of the
     * message view as applicable.
     *
     * @param isShowing Whether the message is visible. {@code true} if shown, {@code false} if
     *     hidden.
     */
    @SuppressWarnings("WrongConstant")
    private void sendPaneChangeAccessibilityEvent(boolean isShowing) {
        AccessibilityEvent event =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        if (isShowing) {
            event.setContentChangeTypes(AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_APPEARED);
        } else {
            event.setContentChangeTypes(
                    AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED);
        }
        AccessibilityState.sendAccessibilityEvent(event);
    }

    private void setOnTitleChanged(@Nullable Runnable runnable) {
        mView.setOnTitleChanged(runnable);
    }
}
