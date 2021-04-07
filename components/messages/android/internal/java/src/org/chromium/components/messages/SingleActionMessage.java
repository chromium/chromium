// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.animation.Animator;
import android.annotation.SuppressLint;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator to show / hide a banner message on given container and delegate events.
 */
public class SingleActionMessage implements MessageStateHandler {
    /**
     * The interface that consumers of SingleActionMessage should implement to receive notification
     * that the message was dismissed.
     */
    @FunctionalInterface
    public interface DismissCallback {
        void invoke(PropertyModel messageProperties, int dismissReason);
    }

    @Nullable
    private MessageBannerCoordinator mMessageBanner;
    private MessageBannerView mView;
    private final MessageContainer mContainer;
    private final PropertyModel mModel;
    private final DismissCallback mDismissHandler;
    private final Supplier<Long> mAutodismissDurationMs;
    private final Supplier<Integer> mMaxTranslationSupplier;
    private final Callback<Animator> mAnimatorStartCallback;

    /**
     * @param container The container holding messages.
     * @param model The PropertyModel with {@link MessageBannerProperties#ALL_KEYS}.
     * @param dismissHandler The {@link DismissCallback} able to dismiss a message by given property
     *         model.
     * @param autodismissDurationMs A {@link Supplier} providing autodismiss duration for message
     *         banner.
     * @param maxTranslationSupplier A {@link Supplier} that supplies the maximum translation Y
     * @param animatorStartCallback The {@link Callback} that will be used by the message banner to
     *         delegate starting the animations to the {@link WindowAndroid}.
     */
    public SingleActionMessage(MessageContainer container, PropertyModel model,
            DismissCallback dismissHandler, Supplier<Integer> maxTranslationSupplier,
            Supplier<Long> autodismissDurationMs, Callback<Animator> animatorStartCallback) {
        mModel = model;
        mContainer = container;
        mDismissHandler = dismissHandler;
        mAutodismissDurationMs = autodismissDurationMs;
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
                    mContainer.getResources(),
                    // clang-format off
                    () -> { mDismissHandler.invoke(mModel, DismissReason.GESTURE); },
                    // clang-format on
                    mAnimatorStartCallback, mAutodismissDurationMs,
                    () -> { mDismissHandler.invoke(mModel, DismissReason.TIMER); });
        }
        mContainer.addMessage(mView);

        // Wait until the message and the container are measured before showing the message. This
        // is required in case the animation set-up requires the height of the container, e.g.
        // showing messages without the top controls visible.
        mContainer.runAfterInitialMessageLayout(mMessageBanner::show);
    }

    /**
     * Hide the message view shown on the given {@link MessageContainer}.
     */
    @Override
    public void hide(boolean animate, Runnable hiddenCallback) {
        Runnable hiddenRunnable = () -> {
            mContainer.removeMessage(mView);
            if (hiddenCallback != null) hiddenCallback.run();
        };
        mMessageBanner.hide(animate, hiddenRunnable);
    }

    /**
     * Remove message from the message queue so that the message will not be shown anymore.
     * @param dismissReason The reason why message is being dismissed.
     */
    @Override
    public void dismiss(@DismissReason int dismissReason) {
        if (mMessageBanner != null) mMessageBanner.dismiss();
        Callback<Integer> onDismissed = mModel.get(MessageBannerProperties.ON_DISMISSED);
        if (onDismissed != null) onDismissed.onResult(dismissReason);
    }

    private void handlePrimaryAction(View v) {
        mModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).run();
        mDismissHandler.invoke(mModel, DismissReason.PRIMARY_ACTION);
    }

    @VisibleForTesting
    long getAutoDismissDuration() {
        return mAutodismissDurationMs.get();
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
