// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for creating a message banner.
 */
class MessageBannerCoordinator {
    private MessageBannerMediator mMediator;

    /**
     * Constructs the message banner.
     *
     * @param view The inflated {@link MessageBannerView}.
     * @param model The model for the message banner.
     */
    MessageBannerCoordinator(MessageBannerView view, PropertyModel model, Context context) {
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        mMediator = new MessageBannerMediator(model, context);
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
}
