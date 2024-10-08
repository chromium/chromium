// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.view.View.OnLongClickListener;

import androidx.annotation.Nullable;

/** The handler for the toolbar long press menu. */
public class ToolbarLongPressMenuHandler {
    @Nullable private OnLongClickListener mOnLongClickListener;

    /**
     * Creates a new {@link ToolbarLongPressMenuHandler}.
     *
     * @param context current context
     */
    public ToolbarLongPressMenuHandler(Context context, boolean isCustomTab) {
        if (ToolbarPositionController.isToolbarPositionCustomizationEnabled(context, isCustomTab)) {
            mOnLongClickListener =
                    (view) -> {
                        displayMenu();
                        return true;
                    };
        }
    }

    /**
     * Return a long-click listener which shows the toolbar popup menu. Return null if toolbar is in
     * CCT or widgets.
     *
     * @return A long-click listener showing the menu.
     */
    protected @Nullable OnLongClickListener getOnLongClickListener() {
        return mOnLongClickListener;
    }

    private void displayMenu() {}
}
