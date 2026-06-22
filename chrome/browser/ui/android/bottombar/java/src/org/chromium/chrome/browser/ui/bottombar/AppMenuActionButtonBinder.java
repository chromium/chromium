// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.AppMenuActionProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the App Menu action button. */
// TODO(crbug.com/524729679): Make this generic to share with toolbar.
@NullMarked
public class AppMenuActionButtonBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        View targetView = ActionButtonBinder.resolveView(view);
        if (targetView instanceof BottomBarAppMenu menuButton) {
            if (AppMenuActionProperties.SHOW_UPDATE_BADGE == propertyKey) {
                boolean showBadge = model.get(AppMenuActionProperties.SHOW_UPDATE_BADGE);
                menuButton.setAppMenuUpdateBadgeVisible(showBadge);
                return;
            } else if (AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE == propertyKey) {
                menuButton.setBadgeUpdateState(
                        model.get(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE));
                return;
            } else if (shouldDelegateToImageButton(propertyKey)) {
                View innerButton = menuButton.getImageButton();
                assert innerButton != null;
                ActionButtonBinder.bind(model, innerButton, propertyKey);
                return;
            }
        }

        ActionButtonBinder.bind(model, view, propertyKey);
    }

    private static boolean shouldDelegateToImageButton(PropertyKey key) {
        return ActionProperties.ICON_ID == key || ActionProperties.ICON_DRAWABLE == key;
    }
}
