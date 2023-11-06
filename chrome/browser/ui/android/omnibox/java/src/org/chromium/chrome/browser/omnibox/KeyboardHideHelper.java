// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.base.WindowDelegate;

/**
 * Helps to detect whether the virtual keyboard was hidden to allow unfocusing of the omnibox.
 *
 * <p>There are no Android APIs to determine the visibility of a soft keyboard, so this class
 * aggressively detects signals that might indicate the keyboard has been hidden.
 */
class KeyboardHideHelper implements ViewTreeObserver.OnGlobalLayoutListener {
    private static final long SOFT_KEYBOARD_HIDDEN_TIMEOUT_MS = 1000;

    private final View mView;
    private final Runnable mOnHideCallback;
    private final Runnable mClearListenerDelayedTask;
    private final Rect mTempRect;

    private WindowDelegate mWindowDelegate;
    private boolean mIsLayoutListenerAttached;
    private int mInitialViewportHeight;

    /**
     * Constructs the helper for hiding the keyboard.
     *
     * @param view The view the keyboard is shown for.
     * @param onHideCallback The callback to be triggered when the keyboard is detected as hidden.
     */
    public KeyboardHideHelper(View view, Runnable onHideCallback) {
        mView = view;
        mOnHideCallback = onHideCallback;
        mClearListenerDelayedTask =
                new Runnable() {
                    @Override
                    public void run() {
                        cleanUp();
                    }
                };
        mTempRect = new Rect();
    }

    /** Initialize the delegate that allows interaction with the Window. */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mWindowDelegate = windowDelegate;
    }

    /**
     * Begin monitoring for keyboard hidden and defocuses the omnibox if it is detected.
     *
     * <p>Only call this method once a strong signal arrives that indicates the keyboard likely will
     * be hidden (i.e. KeyEvent.KEYCODE_BACK in View#onKeyPreIme). Any increase in window size will
     * trigger the hide callback to be notified after this is called. This is meant to be a "good"
     * approximation for user intent to dimiss the keyboard to compensate for the lack of a proper
     * signal from the system.
     */
    public void monitorForKeyboardHidden() {
        cleanUp();

        // If a hardware keyboard is attached, they might be hiding the virtual keyboard, but
        // attempting to continue typing with the hardware keyboard.  Disable unfocusing the
        // omnibox automatically if we detect this case might be possible.
        if (mView.getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY) {
            return;
        }

        if (mWindowDelegate != null) {
            assert mWindowDelegate.getWindowSoftInputMode()
                            != WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING
                    : "SOFT_INPUT_ADJUST_NOTHING prevents detecting window size changes.";
        }

        mView.getViewTreeObserver().addOnGlobalLayoutListener(this);
        mIsLayoutListenerAttached = true;

        mInitialViewportHeight = availableWindowHeight();
        mView.postDelayed(mClearListenerDelayedTask, SOFT_KEYBOARD_HIDDEN_TIMEOUT_MS);
    }

    @Override
    public void onGlobalLayout() {
        if (availableWindowHeight() > mInitialViewportHeight) {
            mOnHideCallback.run();
            cleanUp();
        }
    }

    @VisibleForTesting
    boolean isMonitoringForLayoutChanges() {
        return mIsLayoutListenerAttached;
    }

    private int availableWindowHeight() {
        if (mWindowDelegate == null) {
            return mView.getRootView().getHeight();
        }

        mWindowDelegate.getWindowVisibleDisplayFrame(mTempRect);
        return Math.min(mTempRect.height(), mWindowDelegate.getDecorViewHeight());
    }

    private void cleanUp() {
        if (!mIsLayoutListenerAttached) return;
        mView.removeCallbacks(mClearListenerDelayedTask);
        mView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        mIsLayoutListenerAttached = false;
    }
}
