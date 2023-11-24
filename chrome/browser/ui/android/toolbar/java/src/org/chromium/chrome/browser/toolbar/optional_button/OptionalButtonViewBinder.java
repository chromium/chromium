// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class OptionalButtonViewBinder {
    static void bind(PropertyModel model, OptionalButtonView view, PropertyKey propertyKey) {
        if (OptionalButtonProperties.BUTTON_DATA.equals(propertyKey)) {
            view.updateButtonWithAnimation(model.get(OptionalButtonProperties.BUTTON_DATA));
        } else if (OptionalButtonProperties.IS_ENABLED.equals(propertyKey)) {
            if (view.getButtonView() != null) {
                view.getButtonView().setEnabled(model.get(OptionalButtonProperties.IS_ENABLED));
            }
        } else if (OptionalButtonProperties.TRANSITION_STARTED_CALLBACK.equals(propertyKey)) {
            view.setTransitionStartedCallback(
                    model.get(OptionalButtonProperties.TRANSITION_STARTED_CALLBACK));
        } else if (OptionalButtonProperties.TRANSITION_FINISHED_CALLBACK.equals(propertyKey)) {
            view.setTransitionFinishedCallback(
                    model.get(OptionalButtonProperties.TRANSITION_FINISHED_CALLBACK));
        } else if (OptionalButtonProperties.ON_BEFORE_HIDE_TRANSITION_CALLBACK.equals(
                propertyKey)) {
            view.setOnBeforeHideTransitionCallback(
                    model.get(OptionalButtonProperties.ON_BEFORE_HIDE_TRANSITION_CALLBACK));
        } else if (OptionalButtonProperties.TRANSITION_ROOT.equals(propertyKey)) {
            view.setTransitionRoot(model.get(OptionalButtonProperties.TRANSITION_ROOT));
        } else if (OptionalButtonProperties.ICON_TINT_LIST.equals(propertyKey)) {
            view.setColorStateList(model.get(OptionalButtonProperties.ICON_TINT_LIST));
        } else if (OptionalButtonProperties.ICON_BACKGROUND_COLOR.equals(propertyKey)) {
            view.setBackgroundColorFilter(
                    model.get(OptionalButtonProperties.ICON_BACKGROUND_COLOR));
        } else if (OptionalButtonProperties.ICON_BACKGROUND_ALPHA.equals(propertyKey)) {
            view.setBackgroundAlpha(model.get(OptionalButtonProperties.ICON_BACKGROUND_ALPHA));
        } else if (OptionalButtonProperties.PADDING_START.equals(propertyKey)) {
            view.setPaddingStart(model.get(OptionalButtonProperties.PADDING_START));
        } else if (OptionalButtonProperties.TRANSITION_CANCELLATION_REQUESTED.equals(propertyKey)) {
            if (model.get(OptionalButtonProperties.TRANSITION_CANCELLATION_REQUESTED)) {
                view.cancelTransition();
                model.set(OptionalButtonProperties.TRANSITION_CANCELLATION_REQUESTED, false);
            }
        } else if (OptionalButtonProperties.IS_ANIMATION_ALLOWED_PREDICATE.equals(propertyKey)) {
            view.setIsAnimationAllowedPredicate(
                    model.get(OptionalButtonProperties.IS_ANIMATION_ALLOWED_PREDICATE));
        }
    }
}
