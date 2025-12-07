// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class HomePageButtonsViewBinder {
    static void bind(
            PropertyModel model, HomePageButtonsContainerView view, PropertyKey propertyKey) {
        if (HomePageButtonsProperties.CONTAINER_VISIBILITY.equals(propertyKey)) {
            view.setVisibility(model.get(HomePageButtonsProperties.CONTAINER_VISIBILITY));
        } else if (HomePageButtonsProperties.ACCESSIBILITY_TRAVERSAL_BEFORE.equals(propertyKey)) {
            view.setAccessibilityTraversalBefore(
                    model.get(HomePageButtonsProperties.ACCESSIBILITY_TRAVERSAL_BEFORE));
        } else if (HomePageButtonsProperties.TRANSLATION_Y.equals(propertyKey)) {
            view.setTranslationY(model.get(HomePageButtonsProperties.TRANSLATION_Y));
        } else if (HomePageButtonsProperties.IS_CLICKABLE.equals(propertyKey)) {
            view.setClickable(model.get(HomePageButtonsProperties.IS_CLICKABLE));
        } else if (HomePageButtonsProperties.ON_KEY_LISTENER.equals(propertyKey)) {
            view.setOnKeyListener(model.get(HomePageButtonsProperties.ON_KEY_LISTENER));
        } else if (HomePageButtonsProperties.IS_BUTTON_VISIBLE.equals(propertyKey)) {
            view.setButtonVisibility(
                    model.get(HomePageButtonsProperties.IS_BUTTON_VISIBLE).first,
                    model.get(HomePageButtonsProperties.IS_BUTTON_VISIBLE).second);
        } else if (HomePageButtonsProperties.BUTTON_DATA.equals(propertyKey)) {
            view.updateButtonData(
                    model.get(HomePageButtonsProperties.BUTTON_DATA).first,
                    model.get(HomePageButtonsProperties.BUTTON_DATA).second);
        } else if (HomePageButtonsProperties.BUTTON_BACKGROUND.equals(propertyKey)) {
            view.setButtonBackgroundResource(
                    model.get(HomePageButtonsProperties.BUTTON_BACKGROUND));
        } else if (HomePageButtonsProperties.BUTTON_TINT_LIST.equals(propertyKey)) {
            view.setColorStateList(model.get(HomePageButtonsProperties.BUTTON_TINT_LIST));
        }
    }
}
