// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;

/**
 * This is an abstract class to override keyboard navigation behavior for views.
 */
public abstract class KeyboardNavigationListener implements OnKeyListener {
    public KeyboardNavigationListener() {
        super();
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_TAB && event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.hasNoModifiers()) {
                View forward = getNextFocusForward();
                if (forward != null) return forward.requestFocus();
            } else if (event.isShiftPressed()) {
                View backward = getNextFocusBackward();
                if (backward != null) return backward.requestFocus();
            }
        } else if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP) {
            return handleEnterKeyPress();
        }
        return false;
    }

    /**
     * Get the view to be focused on a TAB click. If you return null, the default key event
     * processing will occur instead of attempting to focus.
     * @return The view to gain focus.
     */
    public View getNextFocusForward() {
        return null;
    }

    /**
     * Get the view to be focused on a Shift + TAB click. If you return null, the default key event
     * processing will occur instead of attempting to focus.
     * @return The view to gain focus.
     */
    public View getNextFocusBackward() {
        return null;
    }

    /**
     * Allows the extending class to special case the enter key press handling.
     * @return Whether the enter key was handled
     */
    protected boolean handleEnterKeyPress() {
        return false;
    }
}
