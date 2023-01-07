// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.content.res.Resources;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for creating a message banner.
 */
@MockedInTests
class MessageBannerCoordinator {
    private final MessageBannerMediator mMediator;
    private final MessageBannerView mView;
    private final PropertyModel mModel;
    private final MessageAutoDismissTimer mTimer;
    private final Supplier<Long> mAutodismissDurationMs;
    private final Runnable mOnTimeUp;

    /**
     * Constructs the message banner.
     *
     * @param view The inflated {@link MessageBannerView}.
     * @param model The model for the message banner.
     * @param maxTranslationSupplier A {@link Supplier} that supplies the maximum translation Y
     *         value the message banner can have as a result of the animations or the gestures.
     * @param resources The {@link Resources}.
     * @param messageDismissed The {@link Runnable} that will run if and when the user dismisses the
     *         message.
     * @param animatorStartCallback The {@link Callback} that will be used to delegate starting the
     *         animations to {@link WindowAndroid} so the message is not clipped as a result of some
     *         Android SurfaceView optimization.
     * @param autodismissDurationMs A {@link Supplier} providing autodismiss duration for message
     *         banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    MessageBannerCoordinator(MessageBannerView view, PropertyModel model,
            Supplier<Integer> maxTranslationSupplier, Resources resources,
            Runnable messageDismissed, Callback<Animator> animatorStartCallback,
            Supplier<Long> autodismissDurationMs, Runnable onTimeUp) {
        mView = view;
        mModel = model;
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator = new MessageBannerMediator(
                model, maxTranslationSupplier, resources, messageDismissed, animatorStartCallback);
        mAutodismissDurationMs = autodismissDurationMs;
        mTimer = new MessageAutoDismissTimer();
        mOnTimeUp = onTimeUp;
        view.setSwipeHandler(mMediator);
        view.setPopupMenuShownListener(
                createPopupMenuShownListener(mTimer, mAutodismissDurationMs.get(), mOnTimeUp));
    }

    /**
     * Creates a {@link PopupMenuShownListener} to handle secondary button popup menu events on the
     * message banner.
     * @param timer The {@link MessageAutoDismissTimer} controlling the message banner dismiss
     *         duration.
     * @param duration The auto dismiss duration for the message banner.
     * @param onTimeUp A {@link Runnable} that will run if and when the auto dismiss timer is up.
     */
    @VisibleForTesting
    PopupMenuShownListener createPopupMenuShownListener(
            MessageAutoDismissTimer timer, long duration, Runnable onTimeUp) {
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
     * @return The animator which shows the message view.
     */
    Animator show(@Position int fromIndex, @Position int toIndex) {
        mView.dismissSecondaryMenuIfShown();
        return mMediator.show(fromIndex, toIndex, () -> {
            if (toIndex != Position.FRONT) {
                setOnTouchRunnable(null);
                setOnTitleChanged(null);
                mTimer.cancelTimer();
            } else {
                setOnTouchRunnable(mTimer::resetTimer);
                announceForAccessibility();
                setOnTitleChanged(() -> {
                    mTimer.resetTimer();
                    announceForAccessibility();
                });
                mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
            }
        });
    }

    /**
     * Hides the message banner.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     * @return The animator which hides the message view.
     */
    Animator hide(boolean animate, Runnable messageHidden) {
        mView.dismissSecondaryMenuIfShown();
        mTimer.cancelTimer();
        // Skip animation if animation has been globally disabled.
        // Otherwise, child animator's listener's onEnd will be called immediately after onStart,
        // even before parent animatorSet's listener's onStart.
        var isAnimationDisabled = Settings.Global.getFloat(mView.getContext().getContentResolver(),
                                          Settings.Global.ANIMATOR_DURATION_SCALE, 1f)
                == 0;
        return mMediator.hide(animate && !isAnimationDisabled, () -> {
            setOnTouchRunnable(null);
            setOnTitleChanged(null);
            messageHidden.run();
        });
    }

    void cancelTimer() {
        mTimer.cancelTimer();
    }

    void startTimer() {
        mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
    }

    void setOnTouchRunnable(Runnable runnable) {
        mMediator.setOnTouchRunnable(runnable);
    }

    private void announceForAccessibility() {
        mView.announceForAccessibility(mModel.get(MessageBannerProperties.TITLE) + " "
                + mView.getResources().getString(R.string.message_screen_position));
    }

    private void setOnTitleChanged(Runnable runnable) {
        mView.setOnTitleChanged(runnable);
    }
}
