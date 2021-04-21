// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.messages.MessageBannerProperties.ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.DESCRIPTION;
import static org.chromium.components.messages.MessageBannerProperties.ICON;
import static org.chromium.components.messages.MessageBannerProperties.ICON_RESOURCE_ID;
import static org.chromium.components.messages.MessageBannerProperties.ICON_TINT_COLOR;
import static org.chromium.components.messages.MessageBannerProperties.ON_SECONDARY_ACTION;
import static org.chromium.components.messages.MessageBannerProperties.ON_TOUCH_RUNNABLE;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_TEXT;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON_CONTENT_DESCRIPTION;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID;
import static org.chromium.components.messages.MessageBannerProperties.TITLE;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_X;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;

import android.annotation.SuppressLint;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder of Message banner.
 */
public class MessageBannerViewBinder {
    @SuppressLint("ClickableViewAccessibility")
    public static void bind(PropertyModel model, MessageBannerView view, PropertyKey propertyKey) {
        if (propertyKey == PRIMARY_BUTTON_TEXT) {
            view.setPrimaryButtonText(model.get(PRIMARY_BUTTON_TEXT));
        } else if (propertyKey == PRIMARY_BUTTON_CLICK_LISTENER) {
            view.setPrimaryButtonClickListener(model.get(PRIMARY_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else if (propertyKey == DESCRIPTION) {
            view.setDescription(model.get(DESCRIPTION));
        } else if (propertyKey == ICON) {
            view.setIcon(model.get(ICON));
        } else if (propertyKey == ICON_RESOURCE_ID) {
            view.setIcon(
                    AppCompatResources.getDrawable(view.getContext(), model.get(ICON_RESOURCE_ID)));
        } else if (propertyKey == ICON_TINT_COLOR) {
            view.setIconTint(model.get(ICON_TINT_COLOR));
        } else if (propertyKey == SECONDARY_ICON) {
            view.setSecondaryIcon(model.get(SECONDARY_ICON));
        } else if (propertyKey == SECONDARY_ICON_RESOURCE_ID) {
            view.setSecondaryIcon(AppCompatResources.getDrawable(
                    view.getContext(), model.get(SECONDARY_ICON_RESOURCE_ID)));
        } else if (propertyKey == SECONDARY_BUTTON_MENU_TEXT) {
            view.setSecondaryButtonMenuText(model.get(SECONDARY_BUTTON_MENU_TEXT));
        } else if (propertyKey == SECONDARY_ICON_CONTENT_DESCRIPTION) {
            view.setSecondaryIconContentDescription(model.get(SECONDARY_ICON_CONTENT_DESCRIPTION));
        } else if (propertyKey == ON_SECONDARY_ACTION) {
            view.setSecondaryActionCallback(model.get(ON_SECONDARY_ACTION));
        } else if (propertyKey == ON_TOUCH_RUNNABLE) {
            Runnable runnable = model.get(ON_TOUCH_RUNNABLE);
            if (runnable == null) {
                view.setOnTouchListener(null);
            } else {
                view.setOnTouchListener((e, v) -> {
                    runnable.run();
                    return false;
                });
            }
        } else if (propertyKey == ALPHA) {
            view.setAlpha(model.get(ALPHA));
        } else if (propertyKey == TRANSLATION_X) {
            view.setTranslationX(model.get(TRANSLATION_X));
        } else if (propertyKey == TRANSLATION_Y) {
            view.setTranslationY(model.get(TRANSLATION_Y));
        }
    }
}
