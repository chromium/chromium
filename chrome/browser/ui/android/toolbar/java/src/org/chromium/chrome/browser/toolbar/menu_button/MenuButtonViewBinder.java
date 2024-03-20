// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import android.view.View;

import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ShowBadgeProperty;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ThemeProperty;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class MenuButtonViewBinder implements ViewBinder<PropertyModel, MenuButton, PropertyKey> {
    // Whether MenuButtonState supplier was set already. The supplier needs to be set before
    // some other properties.
    private boolean mStateSupplierSet;

    @Override
    public void bind(PropertyModel model, MenuButton view, PropertyKey propertyKey) {
        if (propertyKey == MenuButtonProperties.ALPHA) {
            view.setAlpha(model.get(MenuButtonProperties.ALPHA));
        } else if (propertyKey == MenuButtonProperties.APP_MENU_BUTTON_HELPER) {
            view.setAppMenuButtonHelper(model.get(MenuButtonProperties.APP_MENU_BUTTON_HELPER));
        } else if (propertyKey == MenuButtonProperties.CONTENT_DESCRIPTION) {
            view.updateContentDescription(model.get(MenuButtonProperties.CONTENT_DESCRIPTION));
        } else if (propertyKey == MenuButtonProperties.IS_CLICKABLE) {
            view.setClickable(model.get(MenuButtonProperties.IS_CLICKABLE));
        } else if (propertyKey == MenuButtonProperties.IS_HIGHLIGHTING) {
            view.setMenuButtonHighlight(model.get(MenuButtonProperties.IS_HIGHLIGHTING));
        } else if (propertyKey == MenuButtonProperties.IS_VISIBLE) {
            view.setVisibility(
                    model.get(MenuButtonProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == MenuButtonProperties.STATE_SUPPLIER) {
            if (mStateSupplierSet) return;
            view.setStateSupplier(model.get(MenuButtonProperties.STATE_SUPPLIER));
            mStateSupplierSet = true;
        } else if (propertyKey == MenuButtonProperties.SHOW_UPDATE_BADGE) {
            bind(model, view, MenuButtonProperties.STATE_SUPPLIER);
            ShowBadgeProperty showBadgeProperty = model.get(MenuButtonProperties.SHOW_UPDATE_BADGE);
            if (showBadgeProperty.mShowUpdateBadge) {
                view.showAppMenuUpdateBadge(showBadgeProperty.mShouldAnimate);
            } else {
                view.removeAppMenuUpdateBadge(showBadgeProperty.mShouldAnimate);
            }
        } else if (propertyKey == MenuButtonProperties.THEME) {
            bind(model, view, MenuButtonProperties.STATE_SUPPLIER);
            ThemeProperty themeProperty = model.get(MenuButtonProperties.THEME);
            view.onTintChanged(
                    themeProperty.mColorStateList,
                    themeProperty.mColorStateList,
                    themeProperty.mBrandedColorScheme);
        } else if (propertyKey == MenuButtonProperties.TRANSLATION_X) {
            view.setTranslationX(model.get(MenuButtonProperties.TRANSLATION_X));
        }
    }
}
