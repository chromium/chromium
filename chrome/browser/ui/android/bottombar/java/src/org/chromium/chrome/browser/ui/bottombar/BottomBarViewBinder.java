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
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (BottomBarProperties.IS_VISIBLE == propertyKey) {
            // TODO(crbug.com/469429568): Remove if not used after implementation is done.
            view.setVisibility(
                    model.get(BottomBarProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (BottomBarProperties.COLOR_SCHEME == propertyKey) {
            // TODO(crbug.com/504612877): Apply color scheme to buttons.
        } else if (BottomBarProperties.IS_HOME_BUTTON_VISIBLE == propertyKey) {
            View homeContainer = view.findViewById(R.id.home_button_container);
            if (homeContainer != null) {
                homeContainer.setVisibility(
                        model.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE)
                                ? View.VISIBLE
                                : View.GONE);
            }
        }
    }
}
