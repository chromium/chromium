// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionId;
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
            view.setColorScheme(model.get(BottomBarProperties.COLOR_SCHEME));
        } else if (BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE == propertyKey) {
            view.setNewTabBackgroundVisible(
                    model.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));
        } else if (BottomBarProperties.IS_HOME_BUTTON_VISIBLE == propertyKey) {
            view.setButtonVisibility(
                    ActionId.HOME_BUTTON, model.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));
        } else if (BottomBarProperties.IS_GLIC_BUTTON_VISIBLE == propertyKey) {
            view.setButtonVisibility(
                    ActionId.GLIC, model.get(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE));
        } else if (BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE == propertyKey) {
            view.setButtonVisibility(
                    ActionId.NEW_TAB, model.get(BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE));
        } else if (BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE == propertyKey) {
            view.setButtonVisibility(
                    ActionId.TAB_SWITCHER,
                    model.get(BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE));
        } else if (BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE == propertyKey) {
            view.setButtonVisibility(
                    ActionId.APP_MENU, model.get(BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE));
        }
    }
}
