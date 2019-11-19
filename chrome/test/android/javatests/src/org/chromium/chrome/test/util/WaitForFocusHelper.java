// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.View;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.text.MessageFormat;

/**
 * An implementation of View.OnFocusChangeListener which composites an existing listener (if any)
 * and allows tests to wait for focus to occur. To use this, call #acquireFocusForView with the
 * view you want to have focus.
 */
public class WaitForFocusHelper implements View.OnFocusChangeListener {
    private static final String WAITING_FOR_FOCUS_TEMPLATE =
            "Failed waiting for url focus for view [{0}]";

    /**
     * Acquires focus for the given view. Will crash and burn if focus isn't acquired successfully.
     * @param view The view which will be focused.
     */
    public static void acquireFocusForView(View view) {
        try {
            WaitForFocusHelper listener = new WaitForFocusHelper(view.getOnFocusChangeListener());
            view.setOnFocusChangeListener(listener);
            int callCount = listener.getOnFocusCallbackHelper().getCallCount();
            TestThreadUtils.runOnUiThreadBlocking(() -> view.requestFocus());
            if (!view.hasFocus()) {
                listener.getOnFocusCallbackHelper().waitForCallback(
                        MessageFormat.format(WAITING_FOR_FOCUS_TEMPLATE, view), callCount);
            }
        } catch (Exception e) {
            e.printStackTrace();
            assert false : MessageFormat.format(WAITING_FOR_FOCUS_TEMPLATE, view);
        }
        Assert.assertTrue(view.hasFocus());
    }

    private CallbackHelper mOnFocusCallbackHelper;
    private View.OnFocusChangeListener mExistingListener;

    WaitForFocusHelper(View.OnFocusChangeListener existingListener) {
        mOnFocusCallbackHelper = new CallbackHelper();
        mExistingListener = existingListener;
    }

    CallbackHelper getOnFocusCallbackHelper() {
        return mOnFocusCallbackHelper;
    }

    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        if (mExistingListener != null) mExistingListener.onFocusChange(v, hasFocus);
        mOnFocusCallbackHelper.notifyCalled();
    }
}