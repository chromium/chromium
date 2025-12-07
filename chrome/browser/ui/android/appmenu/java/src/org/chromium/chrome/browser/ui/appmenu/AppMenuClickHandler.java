// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.modelutil.PropertyModel;

/** Interface to handle clicks and long-clicks on menu items. */
@NullMarked
public interface AppMenuClickHandler {

    /**
     * Handles clicks on the AppMenu popup.
     *
     * <p>The default implementation works for most cases, so it's not recommended to override it
     * unless you are sure.
     *
     * @see #onItemClick(PropertyModel, MotionEventInfo)
     */
    default void onItemClick(PropertyModel model) {
        onItemClick(model, /* triggeringMotion= */ null);
    }

    /**
     * Handles clicks on the AppMenu popup.
     *
     * @param model The {@link PropertyModel} of the clicked menu item.
     * @param triggeringMotion The {@link MotionEventInfo} that triggered the click; it is {@code
     *     null} if {@link MotionEvent} wasn't available when the click was detected, such as in
     *     {@link android.view.View.OnClickListener}.
     */
    void onItemClick(PropertyModel model, @Nullable MotionEventInfo triggeringMotion);

    /**
     * Handles long clicks on image buttons on the AppMenu popup.
     *
     * @param model The {@link PropertyModel} of the long clicked menu item.
     * @param view The anchor view of the menu item.
     */
    boolean onItemLongClick(PropertyModel model, View view);
}
