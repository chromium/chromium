// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the start surface toolbar properties. */
class StartSurfaceToolbarProperties {
    private StartSurfaceToolbarProperties() {}

    public static final PropertyModel
            .WritableObjectPropertyKey<IncognitoStateProvider> INCOGNITO_STATE_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<IncognitoStateProvider>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> NEW_TAB_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    // Handles the enabled state of the New tab button "+" and the text "New tab".
    public static final PropertyModel.WritableBooleanPropertyKey IS_NEW_TAB_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> IDENTITY_DISC_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> IDENTITY_DISC_IMAGE =
            new PropertyModel.WritableObjectPropertyKey<Drawable>(false);
    public static final PropertyModel.WritableObjectPropertyKey<String> IDENTITY_DISC_DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<String>();
    public static final PropertyModel.WritableBooleanPropertyKey IDENTITY_DISC_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey ACCESSIBILITY_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey MENU_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_VIEW_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_VIEW_TEXT_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey BUTTONS_CLICKABLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_BUTTON_HIGHLIGHT =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey TAB_SWITCHER_BUTTON_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<TabCountProvider> INCOGNITO_TAB_COUNT_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<TabCountProvider>();
    public static final PropertyModel
            .WritableObjectPropertyKey<TabModelSelector> INCOGNITO_TAB_MODEL_SELECTOR =
            new PropertyModel.WritableObjectPropertyKey<TabModelSelector>();

    /** When set to true, move identity disc to the start of the toolbar. Can only set to true. */
    public static final PropertyModel.WritableBooleanPropertyKey IDENTITY_DISC_AT_START =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey INCOGNITO_SWITCHER_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableFloatPropertyKey TRANSLATION_Y =
            new PropertyModel.WritableFloatPropertyKey();
    public static final PropertyModel.WritableFloatPropertyKey ALPHA =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {NEW_TAB_CLICK_HANDLER, IS_NEW_TAB_ENABLED, IS_VISIBLE,
                    INCOGNITO_STATE_PROVIDER, ACCESSIBILITY_ENABLED, MENU_IS_VISIBLE,
                    NEW_TAB_VIEW_IS_VISIBLE, NEW_TAB_VIEW_TEXT_IS_VISIBLE, BUTTONS_CLICKABLE,
                    IDENTITY_DISC_AT_START, INCOGNITO_SWITCHER_VISIBLE, IDENTITY_DISC_IS_VISIBLE,
                    IDENTITY_DISC_CLICK_HANDLER, IDENTITY_DISC_IMAGE, IDENTITY_DISC_DESCRIPTION,
                    NEW_TAB_BUTTON_HIGHLIGHT, TRANSLATION_Y, ALPHA, TAB_SWITCHER_BUTTON_IS_VISIBLE,
                    INCOGNITO_TAB_COUNT_PROVIDER, INCOGNITO_TAB_MODEL_SELECTOR, BACKGROUND_COLOR};
}
