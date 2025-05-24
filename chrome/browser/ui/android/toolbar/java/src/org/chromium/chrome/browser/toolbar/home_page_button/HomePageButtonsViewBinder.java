// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class HomePageButtonsViewBinder {
    static void bind(
            PropertyModel model, HomePageButtonsContainerView view, PropertyKey propertyKey) {
        if (HomePageButtonsProperties.IS_CONTAINER_VISIBLE.equals(propertyKey)) {
            view.setVisibility(
                    model.get(HomePageButtonsProperties.IS_CONTAINER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
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
