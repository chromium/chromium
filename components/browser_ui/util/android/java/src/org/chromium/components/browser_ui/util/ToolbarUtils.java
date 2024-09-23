// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.Toolbar;

/** A helper class for Toolbars. */
public class ToolbarUtils {
    /**
     * A helper that is used to set the visibility of the overflow menu view in a given activity.
     *
     * @param toolbar The menu that may contain the overflow menu item.
     * @param visibility The new visibility of the overflow menu view.
     * @return True if the visibility could be set, false otherwise (e.g. because no menu exists).
     */
    public static boolean setOverflowMenuVisibility(Toolbar toolbar, int visibility) {
        View overflowButton = getOverflowMenuItemFromToolbar(toolbar);
        if (overflowButton == null) return false;
        overflowButton.setVisibility(visibility);
        return true;
    }

    /**
     * A helper that is used to enable/disable the overflow menu view in a given activity.
     *
     * @param toolbar The toolbar that may contain the overflow menu item.
     * @param enabled Whether or not the button will be enabled.
     * @return True if the new state could be set, false otherwise (e.g. because no menu exists).
     */
    public static boolean setOverFlowMenuEnabled(Toolbar toolbar, boolean enabled) {
        View overflowButton = getOverflowMenuItemFromToolbar(toolbar);
        if (overflowButton == null) return false;
        overflowButton.setEnabled(enabled);
        return true;
    }

    /**
     * Finds the menu view in the action bar. Then, finds the overflow button in the menu view. If
     * either is unable to be found, returns null. Otherwise, returns the overflow menu button.
     * TODO(crbug.com/40198147): Rework how we do this by adding an id to the overflow menu button.
     * This would allow us to findViewById(). .
     *
     * @param toolbar The toolbar that may contain the overflow menu item.
     * @return The overflow menu button if found, null otherwise (e.g. no menu exists).
     */
    private static View getOverflowMenuItemFromToolbar(Toolbar toolbar) {
        // Find the menu in the toolbar if it exists. Return null if it does not.
        int i = toolbar.getChildCount();
        ActionMenuView menuView = null;
        while (i-- > 0) {
            if (toolbar.getChildAt(i) instanceof ActionMenuView) {
                menuView = (ActionMenuView) toolbar.getChildAt(i);
                break;
            }
        }
        if (menuView == null) return null;

        // Find the overflow button in the menu if it exists. Return null if it does not.
        View overflowButton = menuView.getChildAt(menuView.getChildCount() - 1);
        if (!isOverflowMenuButton(overflowButton, menuView)) return null;

        return overflowButton;
    }

    /**
     * There is no regular way to access the overflow button of an {@link ActionMenuView}.
     * Checking whether a given view is an {@link ImageView} with the correct icon is an
     * approximation to this issue as the exact icon that the parent menu will set is always known.
     *
     * @param button A view in the |parentMenu| that might be the overflow menu.
     * @param parentMenu The menu that created the overflow button.
     * @return True, if the given button can belong to the overflow menu. False otherwise.
     */
    private static boolean isOverflowMenuButton(View button, ActionMenuView parentMenu) {
        if (button == null) return false;
        if (!(button instanceof ImageView)) {
            return false; // Normal items are usually TextView or LinearLayouts.
        }
        ImageView imageButton = (ImageView) button;
        return imageButton.getDrawable() == parentMenu.getOverflowIcon();
    }
}
