// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.annotation.SuppressLint;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Coordinator to show / hide a banner message on given container and delegate events.
 */
public class SingleActionMessage implements MessageStateHandler {
    private static final long DURATION = 10 * DateUtils.SECOND_IN_MILLIS;
    private static final long DURATION_ON_A11Y = 20 * DateUtils.SECOND_IN_MILLIS;

    private MessageBannerCoordinator mMessageBanner;
    private MessageBannerView mView;
    private final MessageContainer mContainer;
    private final PropertyModel mModel;
    private final Callback<PropertyModel> mDismissHandler;
    private MessageAutoDismissTimer mAutoDismissTimer;
    private final Supplier<Integer> mMaxTranslationSupplier;
    private final AccessibilityUtil mAccessibilityUtil;
    private final Callback<Animator> mAnimatorStartCallback;

    /**
     * @param container The container holding messages.
     * @param model The PropertyModel with {@link
     *         MessageBannerProperties#SINGLE_ACTION_MESSAGE_KEYS}.
     * @param dismissHandler The {@link Callback<PropertyModel>} able to dismiss a message by given
     *         property model.
     * @param maxTranslationSupplier A {@link Supplier} that supplies the maximum translation Y
     * @param accessibilityUtil A util to expose information related to system accessibility state.
     * @param animatorStartCallback The {@link Callback} that will be used by the message banner to
     *         delegate starting the animations to the {@link WindowAndroid}.
     */
    public SingleActionMessage(MessageContainer container, PropertyModel model,
            Callback<PropertyModel> dismissHandler, Supplier<Integer> maxTranslationSupplier,
            AccessibilityUtil accessibilityUtil, Callback<Animator> animatorStartCallback) {
        mModel = model;
        mContainer = container;
        mDismissHandler = dismissHandler;
        mAccessibilityUtil = accessibilityUtil;
        mAutoDismissTimer = new MessageAutoDismissTimer(getAutoDismissDuration());
        mMaxTranslationSupplier = maxTranslationSupplier;
        mAnimatorStartCallback = animatorStartCallback;

        mModel.set(
                MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER, this::handlePrimaryAction);
    }

    /**
     * Show a message view on the given {@link MessageContainer}.
     */
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public void show() {
        if (mMessageBanner == null) {
            mView = (MessageBannerView) LayoutInflater.from(mContainer.getContext())
                            .inflate(R.layout.message_banner_view, mContainer, false);
            mMessageBanner = new MessageBannerCoordinator(mView, mModel, mMaxTranslationSupplier,
                    mContainer.getResources(), mDismissHandler.bind(mModel),
                    mAnimatorStartCallback);
        }
        mContainer.addMessage(mView);

        final Runnable showRunnable = () -> mMessageBanner.show(() -> {
            mMessageBanner.setOnTouchRunnable(mAutoDismissTimer::resetTimer);
            mMessageBanner.announceForAccessibility();
            mAutoDismissTimer.startTimer(() -> { mDismissHandler.onResult(mModel); });
        });

        // Wait until the message and the container are measured before showing the message. This
        // is required in case the animation set-up requires the height of the container, e.g.
        // showing messages without the top controls visible.
        mContainer.runAfterInitialMessageLayout(showRunnable);
    }

    /**
     * Hide the message view shown on the given {@link MessageContainer}.
     */
    @Override
    public void hide(boolean animate, Runnable hiddenCallback) {
        mAutoDismissTimer.cancelTimer();
        mMessageBanner.setOnTouchRunnable(null);
        Runnable hiddenRunnable = () -> {
            mContainer.removeMessage(mView);
            if (hiddenCallback != null) hiddenCallback.run();
        };
        if (animate) {
            mMessageBanner.hide(hiddenRunnable);
        } else {
            hiddenRunnable.run();
        }
    }

    /**
     * Remove message from the message queue so that the message will not be shown anymore.
     */
    @Override
    public void dismiss() {
        mAutoDismissTimer.cancelTimer();
        Runnable onDismissed = mModel.get(MessageBannerProperties.ON_DISMISSED);
        if (onDismissed != null) onDismissed.run();
    }

    private void handlePrimaryAction(View v) {
        mModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).run();
        mDismissHandler.onResult(mModel);
    }

    @VisibleForTesting
    long getAutoDismissDuration() {
        return mAccessibilityUtil.isAccessibilityEnabled() ? DURATION_ON_A11Y : DURATION;
    }

    @VisibleForTesting
    void setMessageBannerForTesting(MessageBannerCoordinator messageBanner) {
        mMessageBanner = messageBanner;
    }

    @VisibleForTesting
    void setViewForTesting(MessageBannerView view) {
        mView = view;
    }
}
