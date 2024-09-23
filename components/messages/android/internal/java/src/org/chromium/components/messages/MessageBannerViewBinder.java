// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.messages.MessageBannerProperties.CONTENT_ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.DESCRIPTION;
import static org.chromium.components.messages.MessageBannerProperties.DESCRIPTION_ICON;
import static org.chromium.components.messages.MessageBannerProperties.DESCRIPTION_MAX_LINES;
import static org.chromium.components.messages.MessageBannerProperties.ELEVATION;
import static org.chromium.components.messages.MessageBannerProperties.ICON;
import static org.chromium.components.messages.MessageBannerProperties.ICON_RESOURCE_ID;
import static org.chromium.components.messages.MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX;
import static org.chromium.components.messages.MessageBannerProperties.ICON_TINT_COLOR;
import static org.chromium.components.messages.MessageBannerProperties.LARGE_ICON;
import static org.chromium.components.messages.MessageBannerProperties.MARGIN_TOP;
import static org.chromium.components.messages.MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK;
import static org.chromium.components.messages.MessageBannerProperties.ON_TOUCH_RUNNABLE;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_TEXT;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_TEXT_MAX_LINES;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE;
import static org.chromium.components.messages.MessageBannerProperties.RESIZE_DESCRIPTION_ICON;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON_CONTENT_DESCRIPTION;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE;
import static org.chromium.components.messages.MessageBannerProperties.SECONDARY_MENU_MAX_SIZE;
import static org.chromium.components.messages.MessageBannerProperties.TITLE;
import static org.chromium.components.messages.MessageBannerProperties.TITLE_CONTENT_DESCRIPTION;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_X;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;
import static org.chromium.components.messages.MessageBannerProperties.VISUAL_HEIGHT;

import android.annotation.SuppressLint;
import android.graphics.Outline;
import android.view.View;
import android.view.ViewOutlineProvider;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder of Message banner. */
public class MessageBannerViewBinder {
    @SuppressLint("ClickableViewAccessibility")
    public static void bind(PropertyModel model, MessageBannerView view, PropertyKey propertyKey) {
        if (propertyKey == PRIMARY_WIDGET_APPEARANCE) {
            view.setPrimaryWidgetAppearance(model.get(PRIMARY_WIDGET_APPEARANCE));
        } else if (propertyKey == PRIMARY_BUTTON_TEXT) {
            view.setPrimaryButtonText(model.get(PRIMARY_BUTTON_TEXT));
        } else if (propertyKey == PRIMARY_BUTTON_TEXT_MAX_LINES) {
            view.setPrimaryButtonTextMaxLines(model.get(PRIMARY_BUTTON_TEXT_MAX_LINES));
        } else if (propertyKey == PRIMARY_BUTTON_CLICK_LISTENER) {
            view.setPrimaryButtonClickListener(model.get(PRIMARY_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else if (propertyKey == TITLE_CONTENT_DESCRIPTION) {
            view.setTitleContentDescription(model.get(TITLE_CONTENT_DESCRIPTION));
        } else if (propertyKey == DESCRIPTION) {
            view.setDescriptionText(model.get(DESCRIPTION));
        } else if (propertyKey == DESCRIPTION_ICON) {
            view.setDescriptionIcon(model.get(DESCRIPTION_ICON));
            view.enableDescriptionIconIntrinsicDimensions(model.get(RESIZE_DESCRIPTION_ICON));
        } else if (propertyKey == RESIZE_DESCRIPTION_ICON) {
            view.enableDescriptionIconIntrinsicDimensions(model.get(RESIZE_DESCRIPTION_ICON));
        } else if (propertyKey == DESCRIPTION_MAX_LINES) {
            view.setDescriptionMaxLines(model.get(DESCRIPTION_MAX_LINES));
        } else if (propertyKey == ICON) {
            view.setIcon(model.get(ICON));
        } else if (propertyKey == ICON_RESOURCE_ID) {
            view.setIcon(
                    AppCompatResources.getDrawable(view.getContext(), model.get(ICON_RESOURCE_ID)));
        } else if (propertyKey == ICON_TINT_COLOR) {
            view.setIconTint(model.get(ICON_TINT_COLOR));
        } else if (propertyKey == ICON_ROUNDED_CORNER_RADIUS_PX) {
            view.setIconCornerRadius(model.get(ICON_ROUNDED_CORNER_RADIUS_PX));
        } else if (propertyKey == LARGE_ICON) {
            view.enableLargeIcon(model.get(LARGE_ICON));
        } else if (propertyKey == SECONDARY_ICON) {
            view.setSecondaryIcon(model.get(SECONDARY_ICON));
        } else if (propertyKey == SECONDARY_ICON_RESOURCE_ID) {
            view.setSecondaryIcon(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(SECONDARY_ICON_RESOURCE_ID)));
        } else if (propertyKey == SECONDARY_BUTTON_MENU_TEXT) {
            view.setSecondaryButtonMenuText(model.get(SECONDARY_BUTTON_MENU_TEXT));
        } else if (propertyKey == SECONDARY_MENU_BUTTON_DELEGATE) {
            view.setSecondaryMenuButtonDelegate(model.get(SECONDARY_MENU_BUTTON_DELEGATE));
        } else if (propertyKey == SECONDARY_MENU_MAX_SIZE) {
            view.setSecondaryMenuMaxSize(model.get(SECONDARY_MENU_MAX_SIZE));
        } else if (propertyKey == SECONDARY_ICON_CONTENT_DESCRIPTION) {
            view.setSecondaryIconContentDescription(
                    model.get(SECONDARY_ICON_CONTENT_DESCRIPTION), false);
        } else if (propertyKey == ON_SECONDARY_BUTTON_CLICK) {
            view.setSecondaryActionCallback(model.get(ON_SECONDARY_BUTTON_CLICK));
        } else if (propertyKey == ON_TOUCH_RUNNABLE) {
            Runnable runnable = model.get(ON_TOUCH_RUNNABLE);
            if (runnable == null) {
                view.setOnTouchListener(null);
            } else {
                view.setOnTouchListener(
                        (e, v) -> {
                            runnable.run();
                            return false;
                        });
            }
        } else if (propertyKey == CONTENT_ALPHA) {
            for (int i = 0; i < view.getChildCount(); i++) {
                view.getChildAt(i).setAlpha(model.get(CONTENT_ALPHA));
            }
        } else if (propertyKey == VISUAL_HEIGHT) {
            final float p = model.get(VISUAL_HEIGHT);
            if (p == 1) {
                view.setClipToOutline(false);
                // reset to its default outline provider.
                view.setOutlineProvider(ViewOutlineProvider.BACKGROUND);
            } else {
                ViewOutlineProvider mViewOutlineProvider =
                        new ViewOutlineProvider() {
                            @Override
                            public void getOutline(final View view, final Outline outline) {
                                float cornerRadius =
                                        view.getResources()
                                                .getDimensionPixelSize(
                                                        R.dimen.message_banner_radius);
                                outline.setRoundRect(
                                        0,
                                        0,
                                        view.getWidth(),
                                        (int) (view.getHeight() * p),
                                        cornerRadius);
                            }
                        };
                view.setOutlineProvider(mViewOutlineProvider);
                view.setClipToOutline(true);
            }
        } else if (propertyKey == TRANSLATION_X) {
            view.setTranslationX(model.get(TRANSLATION_X));
        } else if (propertyKey == TRANSLATION_Y) {
            view.setTranslationY(model.get(TRANSLATION_Y));
        } else if (propertyKey == ELEVATION) {
            view.setElevation(model.get(ELEVATION));
        } else if (propertyKey == MARGIN_TOP) {
            view.setMarginTop(model.get(MARGIN_TOP));
        }
    }
}
