// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.base.Callback;
import org.chromium.base.annotations.MockedInTests;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
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
        ViewCompat.replaceAccessibilityAction(
                view, AccessibilityActionCompat.ACTION_DISMISS, null, (v, c) -> {
                    messageDismissed.run();
                    return false;
                });
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
     */
    void show() {
        mMediator.show(() -> {
            setOnTouchRunnable(mTimer::resetTimer);
            announceForAccessibility();
            setOnTitleChanged(() -> {
                mTimer.resetTimer();
                announceForAccessibility();
            });
            mTimer.startTimer(mAutodismissDurationMs.get(), mOnTimeUp);
        });
    }

    /**
     * Hides the message banner.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     */
    void hide(boolean animate, Runnable messageHidden) {
        mTimer.cancelTimer();
        mMediator.hide(animate, () -> {
            setOnTouchRunnable(null);
            setOnTitleChanged(null);
            messageHidden.run();
        });
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
