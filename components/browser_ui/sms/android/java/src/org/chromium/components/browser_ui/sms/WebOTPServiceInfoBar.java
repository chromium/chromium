// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.sms;

import android.app.Activity;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

/** An InfoBar that asks for the user's permission to share the SMS with the page. */
public class WebOTPServiceInfoBar extends ConfirmInfoBar {
    private static final String TAG = "WebOTPServiceInfoBar";
    private static final boolean DEBUG = false;
    private String mMessage;
    private WindowAndroid mWindowAndroid;
    private Long mKeyboardDismissedTime;

    @VisibleForTesting
    @CalledByNative
    public static WebOTPServiceInfoBar create(
            WindowAndroid windowAndroid,
            int iconId,
            String title,
            String message,
            String okButtonLabel) {
        if (DEBUG) Log.d(TAG, "WebOTPServiceInfoBar.create()");
        return new WebOTPServiceInfoBar(windowAndroid, iconId, title, message, okButtonLabel);
    }

    private WebOTPServiceInfoBar(
            WindowAndroid windowAndroid,
            int iconId,
            String title,
            String message,
            String okButtonLabel) {
        super(
                iconId,
                R.color.infobar_icon_drawable_color,
                /* iconBitmap= */ null,
                /* message= */ title,
                /* linkText= */ null,
                okButtonLabel,
                /* secondaryButtonText= */ null);
        mMessage = message;
        mWindowAndroid = windowAndroid;
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.USER_TRIGGERED;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        WebOTPServiceUma.recordInfobarAction(WebOTPServiceUma.InfobarAction.SHOWN);

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            View focusedView = activity.getCurrentFocus();
            KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                    KeyboardVisibilityDelegate.getInstance();
            if (focusedView != null
                    && keyboardVisibilityDelegate.isKeyboardShowing(activity, focusedView)) {
                keyboardVisibilityDelegate.hideKeyboard(focusedView);
                WebOTPServiceUma.recordInfobarAction(
                        WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED);
                mKeyboardDismissedTime = SystemClock.uptimeMillis();
            }
        }

        InfoBarControlLayout control = layout.addControlLayout();
        control.addDescription(mMessage);
    }

    @Override
    public void onCloseButtonClicked() {
        super.onCloseButtonClicked();

        if (mKeyboardDismissedTime != null) {
            WebOTPServiceUma.recordCancelTimeAfterKeyboardDismissal(
                    SystemClock.uptimeMillis() - mKeyboardDismissedTime);
        }
    }
}
