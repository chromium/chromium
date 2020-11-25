// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.res.Resources;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for creating a message banner.
 */
class MessageBannerCoordinator {
    private final MessageBannerMediator mMediator;

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
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator = new MessageBannerMediator(
                model, maxTranslationSupplier, resources, messageDismissed);
        view.setSwipeHandler(mMediator);
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
}
