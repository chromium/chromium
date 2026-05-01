// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the bottom bar. */
@NullMarked
public class BottomBarViewBinder {
    public static void bind(PropertyModel model, BottomBarView view, PropertyKey propertyKey) {
        if (BottomBarProperties.IS_VISIBLE == propertyKey) {
            // TODO(crbug.com/469429568): Remove if not used after implementation is done.
            view.setVisibility(
                    model.get(BottomBarProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (BottomBarProperties.COLOR_SCHEME == propertyKey) {
            // This property is also used to also applies updates to the action buttons via another
            // binder in BottomBarCoordinator.
            view.setColorScheme(model.get(BottomBarProperties.COLOR_SCHEME));
        } else if (BottomBarProperties.IS_HOME_BUTTON_VISIBLE == propertyKey) {
            view.setHomeButtonVisible(model.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));
        } else if (BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE == propertyKey) {
            view.setNewTabBackgroundVisible(
                    model.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));
        } else if (BottomBarProperties.IS_GLIC_BUTTON_VISIBLE == propertyKey) {
            View extraContainer = view.findViewById(R.id.extra_button_container);
            if (extraContainer != null) {
                extraContainer.setVisibility(
                        model.get(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE)
                                ? View.VISIBLE
                                : View.GONE);
            }
        }
    }
}
