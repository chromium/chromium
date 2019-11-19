// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.View;

/**
 * A helper class for a menu button to decide when to show the app menu and forward touch events.
 *
 * Use {@link AppMenuHandler#createAppMenuButtonHelper()} to create a new instance, then pass the
 * set the AppMenuButtonHelper instance as a TouchListener for a menu button.
 */
public interface AppMenuButtonHelper extends View.OnTouchListener {
    /**
     * @param showsFromBottom Whether the menu shows from the bottom by default.
     */
    void setMenuShowsFromBottom(boolean showsFromBottom);

    /**
     * @return Whether app menu is active. That is, AppMenu is showing or menu button is consuming
     *         touch events to prepare AppMenu showing.
     */
    boolean isAppMenuActive();

    /**
     * Handle the key press event on a menu button.
     * @param view View that received the enter key press event.
     * @return Whether the app menu was shown as a result of this action.
     */
    boolean onEnterKeyPress(View view);

    /**
     * @return An accessibility delegate for the menu button view.
     */
    View.AccessibilityDelegate getAccessibilityDelegate();

    /**
     * TODO(https://crbug.com/956260): Try to unify with AppMenuObserver#onMenuVisibilityChanged()?
     * @param onAppMenuShownListener This is called when the app menu is shown by this class.
     */
    void setOnAppMenuShownListener(Runnable onAppMenuShownListener);

    /**
     * Set a runnable for click events on the menu button. This runnable is triggered with the down
     * motion event rather than click specifically. This is primarily used to track interaction with
     * the menu button.
     * @param clickRunnable The {@link Runnable} to be executed on a down event.
     */
    void setOnClickRunnable(Runnable clickRunnable);
}
