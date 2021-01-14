// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.res.Resources;
import android.view.View;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for creating a message banner.
 */
class MessageBannerCoordinator {
    private final MessageBannerMediator mMediator;
    private final View mView;
    private final PropertyModel mModel;

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
     */
    MessageBannerCoordinator(MessageBannerView view, PropertyModel model,
            Supplier<Integer> maxTranslationSupplier, Resources resources,
            Runnable messageDismissed) {
        mView = view;
        mModel = model;
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator = new MessageBannerMediator(
                model, maxTranslationSupplier, resources, messageDismissed);
        view.setSwipeHandler(mMediator);
        ViewCompat.replaceAccessibilityAction(
                view, AccessibilityActionCompat.ACTION_DISMISS, null, (v, c) -> {
                    messageDismissed.run();
                    return false;
                });
    }

    /**
     * Shows the message banner.
     * @param messageShown The {@link Runnable} that will run once the message banner is shown.
     */
    void show(Runnable messageShown) {
        mMediator.show(messageShown);
    }

    /**
     * Hides the message banner.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     */
    void hide(Runnable messageHidden) {
        mMediator.hide(messageHidden);
    }

    void setOnTouchRunnable(Runnable runnable) {
        mMediator.setOnTouchRunnable(runnable);
    }

    void announceForAccessibility() {
        mView.announceForAccessibility(mModel.get(MessageBannerProperties.TITLE) + " "
                + mView.getResources().getString(R.string.message_screen_position));
    }
}
