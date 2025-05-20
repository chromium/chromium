// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/** A delegate to handle menu item selection. */
@NullMarked
public interface AppMenuDelegate {

    /**
     * Called whenever an item in the app menu is selected.
     *
     * <p>The default implementation works for most cases, so it's not recommended to override it
     * unless you are sure.
     *
     * @see #onOptionsItemSelected(int, Bundle, MotionEventInfo)
     */
    default boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        return onOptionsItemSelected(itemId, menuItemData, /* triggeringMotion= */ null);
    }

    /**
     * Called whenever an item in the app menu is selected. See {@link
     * android.app.Activity#onOptionsItemSelected(MenuItem)}.
     *
     * @param itemId The id of the menu item that was selected.
     * @param menuItemData Extra data associated with the menu item. May be null.
     * @param triggeringMotion The {@link MotionEvent} that triggered the click; it is {@code null}
     *     if {@link MotionEvent} wasn't available when the click was detected, such as in {@link
     *     android.view.View.OnClickListener}.
     */
    boolean onOptionsItemSelected(
            int itemId, @Nullable Bundle menuItemData, @Nullable MotionEventInfo triggeringMotion);

    /**
     * @return {@link AppMenuPropertiesDelegate} instance that the {@link AppMenuHandlerImpl} should
     *     be using.
     */
    AppMenuPropertiesDelegate createAppMenuPropertiesDelegate();
}
