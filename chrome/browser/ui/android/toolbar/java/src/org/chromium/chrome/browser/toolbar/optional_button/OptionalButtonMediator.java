// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonProperties.OnBeforeWidthTransitionCallback;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class OptionalButtonMediator {
    private final PropertyModel mModel;

    public OptionalButtonMediator(PropertyModel model) {
        mModel = model;
    }

    void updateButton(@Nullable ButtonData buttonData) {
        mModel.set(OptionalButtonProperties.BUTTON_DATA, buttonData);
        if (buttonData != null) {
            mModel.set(OptionalButtonProperties.IS_ENABLED, buttonData.isEnabled());
        }
    }

    void setTransitionStartedCallback(Callback<Integer> transitionStartedCallback) {
        mModel.set(OptionalButtonProperties.TRANSITION_STARTED_CALLBACK, transitionStartedCallback);
    }

    void setIconForegroundColor(@Nullable ColorStateList colorStateList) {
        mModel.set(OptionalButtonProperties.ICON_TINT_LIST, colorStateList);
    }

    void setOnBeforeWidthTransitionCallback(OnBeforeWidthTransitionCallback callback) {
        mModel.set(OptionalButtonProperties.ON_BEFORE_WIDTH_TRANSITION_CALLBACK, callback);
    }

    void setBackgroundColorFilter(@ColorInt int backgroundColor) {
        mModel.set(OptionalButtonProperties.ICON_BACKGROUND_COLOR, backgroundColor);
    }

    void setBackgroundAlpha(int alpha) {
        mModel.set(OptionalButtonProperties.ICON_BACKGROUND_ALPHA, alpha);
    }

    void setIsIncognitoBranded(boolean isIncognitoBranded) {
        mModel.set(OptionalButtonProperties.IS_INCOGNITO_BRANDED, isIncognitoBranded);
    }

    void setCanChangeVisibility(boolean canChange) {
        mModel.set(OptionalButtonProperties.CAN_CHANGE_VISIBILITY, canChange);
    }

    public void setOnBeforeHideTransitionCallback(Runnable onBeforeHideTransitionCallback) {
        mModel.set(
                OptionalButtonProperties.ON_BEFORE_HIDE_TRANSITION_CALLBACK,
                onBeforeHideTransitionCallback);
    }

    public void setPaddingStart(int paddingStart) {
        mModel.set(OptionalButtonProperties.PADDING_START, paddingStart);
    }

    public void setCollapsedStateWidth(int width) {
        mModel.set(OptionalButtonProperties.COLLAPSED_STATE_WIDTH, width);
    }

    public void cancelTransition() {
        mModel.set(OptionalButtonProperties.TRANSITION_CANCELLATION_REQUESTED, true);
    }
}
