// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/**
 * A controller to register/unregister {@link MenuOrKeyboardActionHandler} for menu or keyboard
 * actions and execute them.
 */
@NullMarked
public interface MenuOrKeyboardActionController {
    /**
     * A handler for menu or keyboard actions. Register via
     * {@link #registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler)}.
     */
    interface MenuOrKeyboardActionHandler {
        /**
         * Handles menu item selection and keyboard shortcuts.
         *
         * @param id The ID of the selected menu item (defined in main_menu.xml) or keyboard
         *           shortcut (defined in values.xml).
         * @param fromMenu Whether this was triggered from the menu.
         * @return Whether the action was handled.
         */
        boolean handleMenuOrKeyboardAction(int id, boolean fromMenu);
    }

    /** @param handler A new {@link MenuOrKeyboardActionHandler} to register. */
    void registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler);

    /** @param handler A {@link MenuOrKeyboardActionHandler} to unregister. */
    void unregisterMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler);

    /**
     * Handles menu item selection and keyboard shortcuts.
     *
     * <p>The default implementation works for most cases, so it's not recommended to override it
     * unless you are sure.
     *
     * @see #onMenuOrKeyboardAction(int, boolean, MotionEventInfo)
     */
    default boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        return onMenuOrKeyboardAction(id, fromMenu, /* triggeringMotion= */ null);
    }

    /**
     * Handles menu item selection and keyboard shortcuts.
     *
     * @param id The ID of the selected menu item (defined in main_menu.xml) or keyboard shortcut
     *     (defined in values.xml).
     * @param fromMenu Whether this was triggered from the menu.
     * @param triggeringMotion The {@link MotionEventInfo} that triggered the action; it is {@code
     *     null} if {@link MotionEvent} wasn't available when the action was detected, such as in
     *     {@link android.view.View.OnClickListener}.
     * @return Whether the action was handled.
     */
    boolean onMenuOrKeyboardAction(
            int id, boolean fromMenu, @Nullable MotionEventInfo triggeringMotion);
}
