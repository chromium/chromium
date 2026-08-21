// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntDefPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the bottom bar. */
@NullMarked
public class BottomBarProperties {
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableIntDefPropertyKey<BrandedColorScheme> COLOR_SCHEME =
            new WritableIntDefPropertyKey<>(BrandedColorScheme.APP_DEFAULT);
    public static final WritableBooleanPropertyKey IS_NEW_TAB_BACKGROUND_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_HOME_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_EXTRA_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey EXTRA_BUTTON_ACTION_ID =
            new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey IS_NEW_TAB_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_TAB_SWITCHER_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_APP_MENU_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                COLOR_SCHEME,
                IS_NEW_TAB_BACKGROUND_VISIBLE,
                IS_HOME_BUTTON_VISIBLE,
                IS_EXTRA_BUTTON_VISIBLE,
                EXTRA_BUTTON_ACTION_ID,
                IS_NEW_TAB_BUTTON_VISIBLE,
                IS_TAB_SWITCHER_BUTTON_VISIBLE,
                IS_APP_MENU_BUTTON_VISIBLE,
            };
}
