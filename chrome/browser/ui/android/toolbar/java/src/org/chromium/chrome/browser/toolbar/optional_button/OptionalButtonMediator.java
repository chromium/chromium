// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.ColorStateList;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;

class OptionalButtonMediator {
    private final PropertyModel mModel;

    public OptionalButtonMediator(PropertyModel model) {
        mModel = model;
    }

    void updateButton(ButtonData buttonData) {
        mModel.set(OptionalButtonProperties.BUTTON_DATA, buttonData);
        if (buttonData != null) {
            mModel.set(OptionalButtonProperties.IS_ENABLED, buttonData.isEnabled());
        }
    }

    void setTransitionRoot(ViewGroup transitionRoot) {
        mModel.set(OptionalButtonProperties.TRANSITION_ROOT, transitionRoot);
    }

    void setTransitionStartedCallback(Callback<Integer> transitionStartedCallback) {
        mModel.set(OptionalButtonProperties.TRANSITION_STARTED_CALLBACK, transitionStartedCallback);
    }

    void setTransitionFinishedCallback(Callback<Integer> transitionFinishedCallback) {
        mModel.set(
                OptionalButtonProperties.TRANSITION_FINISHED_CALLBACK, transitionFinishedCallback);
    }

    void setIconForegroundColor(ColorStateList colorStateList) {
        mModel.set(OptionalButtonProperties.ICON_TINT_LIST, colorStateList);
    }

    void setBackgroundColorFilter(int backgroundColor) {
        mModel.set(OptionalButtonProperties.ICON_BACKGROUND_COLOR, backgroundColor);
    }

    public void setOnBeforeHideTransitionCallback(Runnable onBeforeHideTransitionCallback) {
        mModel.set(OptionalButtonProperties.ON_BEFORE_HIDE_TRANSITION_CALLBACK,
                onBeforeHideTransitionCallback);
    }

    public void setPaddingStart(int paddingStart) {
        mModel.set(OptionalButtonProperties.PADDING_START, paddingStart);
    }

    public void cancelTransition() {
        mModel.set(OptionalButtonProperties.TRANSITION_CANCELLATION_REQUESTED, true);
    }

    public void setIsAnimationAllowedPredicate(BooleanSupplier isAnimationAllowedPredicate) {
        mModel.set(OptionalButtonProperties.IS_ANIMATION_ALLOWED_PREDICATE,
                isAnimationAllowedPredicate);
    }
}
