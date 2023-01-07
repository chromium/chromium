// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;

/**
 * Coordinator responsible for enabling or disabling the soft keyboard.
 */
class AssistantKeyboardCoordinator {
    private final Activity mActivity;
    private final KeyboardVisibilityDelegate mKeyboardDelegate;
    private final View mRootView;
    private final KeyboardVisibilityListener mKeyboardVisibilityListener =
            this::onKeyboardVisibilityChanged;
    private boolean mAllowShowingSoftKeyboard = true;
    private Delegate mDelegate;
    private final BottomSheetController mBottomSheetController;

    interface Delegate {
        void onKeyboardVisibilityChanged(boolean visible);
    }

    // TODO(b/173103628): refactor and inject the keyboard delegate directly.
    AssistantKeyboardCoordinator(Activity activity,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate, View rootView,
            AssistantModel model, Delegate delegate, BottomSheetController controller) {
        mActivity = activity;
        mKeyboardDelegate = keyboardVisibilityDelegate;
        mRootView = rootView;
        mDelegate = delegate;
        mBottomSheetController = controller;

        model.addObserver((source, propertyKey) -> {
            if (AssistantModel.VISIBLE == propertyKey) {
                if (model.get(AssistantModel.VISIBLE)) {
                    enableListenForKeyboardVisibility(true);
                } else {
                    enableListenForKeyboardVisibility(false);
                }
            } else if (AssistantModel.ALLOW_SOFT_KEYBOARD == propertyKey) {
                allowShowingSoftKeyboard(model.get(AssistantModel.ALLOW_SOFT_KEYBOARD));
            }
        });
    }

    /** Returns whether the keyboard is currently shown. */
    boolean isKeyboardShown() {
        return mKeyboardDelegate.isKeyboardShowing(mActivity, mRootView);
    }

    /** Returns whether the BottomSheet is currently shown. */
    private boolean isBottomSheetShown() {
        return mBottomSheetController.getSheetState() != SheetState.HIDDEN;
    }

    /** Hides the keyboard. */
    void hideKeyboard() {
        mKeyboardDelegate.hideKeyboard(mRootView);
    }

    /** Hides the keyboard after a delay if the focus is not on a TextView */
    void hideKeyboardIfFocusNotOnText() {
        if (!(mActivity.getCurrentFocus() instanceof TextView)) {
            hideKeyboard();
        }
    }

    /** Start or stop listening for keyboard visibility changes. */
    private void enableListenForKeyboardVisibility(boolean enabled) {
        if (enabled) {
            mKeyboardDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        } else {
            mKeyboardDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        }
    }

    /** Set soft keyboard allowed state. */
    private void allowShowingSoftKeyboard(boolean allowed) {
        mAllowShowingSoftKeyboard = allowed;
        if (!allowed && isBottomSheetShown()) {
            hideKeyboard();
        }
    }

    /** If the keyboard shows up and is not allowed, hide it. */
    private void onKeyboardVisibilityChanged(boolean isShowing) {
        mDelegate.onKeyboardVisibilityChanged(isShowing);
        if (isShowing && !mAllowShowingSoftKeyboard && isBottomSheetShown()) {
            hideKeyboard();
        }
    }
}
