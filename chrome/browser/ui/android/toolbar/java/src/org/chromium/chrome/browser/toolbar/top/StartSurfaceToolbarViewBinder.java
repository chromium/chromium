// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ALPHA;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_COUNT_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_MODEL_SELECTOR;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_NEW_TAB_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_HIGHLIGHT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_TEXT_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TAB_SWITCHER_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TRANSLATION_Y;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the tasks surface view.
class StartSurfaceToolbarViewBinder {
    public static void bind(
            PropertyModel model, StartSurfaceToolbarView view, PropertyKey propertyKey) {
        if (propertyKey == ACCESSIBILITY_ENABLED) {
            view.onAccessibilityStatusChanged(model.get(ACCESSIBILITY_ENABLED));
        } else if (propertyKey == BUTTONS_CLICKABLE) {
            view.setButtonClickableState(model.get(BUTTONS_CLICKABLE));
        } else if (propertyKey == INCOGNITO_SWITCHER_VISIBLE) {
            view.setIncognitoToggleTabVisibility((Boolean) model.get(INCOGNITO_SWITCHER_VISIBLE));
        } else if (propertyKey == IDENTITY_DISC_AT_START) {
            view.setIdentityDiscAtStart(model.get(IDENTITY_DISC_AT_START));
        } else if (propertyKey == IDENTITY_DISC_CLICK_HANDLER) {
            view.setIdentityDiscClickHandler(model.get(IDENTITY_DISC_CLICK_HANDLER));
        } else if (propertyKey == IDENTITY_DISC_DESCRIPTION) {
            view.setIdentityDiscContentDescription(model.get(IDENTITY_DISC_DESCRIPTION));
        } else if (propertyKey == IDENTITY_DISC_IMAGE) {
            view.setIdentityDiscImage(model.get(IDENTITY_DISC_IMAGE));
        } else if (propertyKey == IDENTITY_DISC_IS_VISIBLE) {
            view.setIdentityDiscVisibility(model.get(IDENTITY_DISC_IS_VISIBLE));
        } else if (propertyKey == INCOGNITO_STATE_PROVIDER) {
            view.setIncognitoStateProvider(model.get(INCOGNITO_STATE_PROVIDER));
        } else if (propertyKey == IS_VISIBLE) {
            view.setToolbarVisibility(model.get(IS_VISIBLE));
        } else if (propertyKey == MENU_IS_VISIBLE) {
            view.setMenuButtonVisibility(model.get(MENU_IS_VISIBLE));
        } else if (propertyKey == NEW_TAB_CLICK_HANDLER) {
            view.setOnNewTabClickHandler(model.get(NEW_TAB_CLICK_HANDLER));
        } else if (propertyKey == IS_NEW_TAB_ENABLED) {
            view.setNewTabEnabled(model.get(IS_NEW_TAB_ENABLED));
        } else if (propertyKey == NEW_TAB_BUTTON_HIGHLIGHT) {
            view.setNewTabButtonHighlight(model.get(NEW_TAB_BUTTON_HIGHLIGHT));
        } else if (propertyKey == NEW_TAB_VIEW_IS_VISIBLE) {
            view.setNewTabViewVisibility(model.get(NEW_TAB_VIEW_IS_VISIBLE));
        } else if (propertyKey == NEW_TAB_VIEW_TEXT_IS_VISIBLE) {
            view.setNewTabViewTextVisibility(model.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        } else if (propertyKey == TRANSLATION_Y) {
            view.setTranslationY(model.get(TRANSLATION_Y));
        } else if (propertyKey == ALPHA) {
            view.setAlpha(model.get(ALPHA));
        } else if (propertyKey == TAB_SWITCHER_BUTTON_IS_VISIBLE) {
            view.setTabSwitcherButtonVisibility(model.get(TAB_SWITCHER_BUTTON_IS_VISIBLE));
        } else if (propertyKey == INCOGNITO_TAB_COUNT_PROVIDER) {
            view.setTabCountProvider(model.get(INCOGNITO_TAB_COUNT_PROVIDER));
        } else if (propertyKey == INCOGNITO_TAB_MODEL_SELECTOR) {
            view.setTabModelSelector(model.get(INCOGNITO_TAB_MODEL_SELECTOR));
        } else if (propertyKey == BACKGROUND_COLOR) {
            view.setToolbarBackgroundColor(model.get(BACKGROUND_COLOR));
        }
    }
}
