// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.view.LayoutInflater;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator to show / hide a banner message on given container and delegate events.
 */
public class SingleActionMessage implements MessageStateHandler {
    private MessageBannerView mView;
    private final MessageContainer mContainer;
    private final PropertyModel mModel;

    /**
     * @param container The container holding messages.
     * @param model The PropertyModel with {@link
     *         MessageBannerProperties#SINGLE_ACTION_MESSAGE_KEYS}.
     */
    public SingleActionMessage(MessageContainer container, PropertyModel model) {
        mModel = model;
        mContainer = container;
    }

    /**
     * Show a message view on the given {@link MessageContainer}.
     */
    @Override
    public void show() {
        if (mView == null) {
            mView = (MessageBannerView) LayoutInflater.from(mContainer.getContext())
                            .inflate(R.layout.message_banner_view, mContainer, false);
            PropertyModelChangeProcessor.create(mModel, mView, MessageBannerViewBinder::bind);
        }
        mContainer.addMessage(mView);
    }

    /**
     * Hide the message view shown on the given {@link MessageContainer}.
     */
    @Override
    public void hide() {
        mContainer.removeMessage(mView);
    }

    /**
     * Remove message from the message queue so that the message will not be shown anymore.
     */
    @Override
    public void dismiss() {
        Runnable onDismissed = mModel.get(MessageBannerProperties.ON_DISMISSED);
        if (onDismissed != null) onDismissed.run();
    }
}
