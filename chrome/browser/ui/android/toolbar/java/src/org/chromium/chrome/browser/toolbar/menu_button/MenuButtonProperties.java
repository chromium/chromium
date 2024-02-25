// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import android.content.res.ColorStateList;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class MenuButtonProperties {
    static class ThemeProperty {
        @NonNull public ColorStateList mColorStateList;
        public @BrandedColorScheme int mBrandedColorScheme;

        public ThemeProperty(
                @NonNull ColorStateList colorStateList,
                @BrandedColorScheme int brandedColorScheme) {
            mColorStateList = colorStateList;
            mBrandedColorScheme = brandedColorScheme;
        }
    }

    static class ShowBadgeProperty {
        public boolean mShowUpdateBadge;
        public boolean mShouldAnimate;

        public ShowBadgeProperty(boolean showUpdateBadge, boolean shouldAnimate) {
            mShowUpdateBadge = showUpdateBadge;
            mShouldAnimate = shouldAnimate;
        }
    }

    public static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    public static final WritableObjectPropertyKey<AppMenuButtonHelper> APP_MENU_BUTTON_HELPER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_CLICKABLE = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_HIGHLIGHTING =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Supplier<MenuButtonState>> STATE_SUPPLIER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ShowBadgeProperty> SHOW_UPDATE_BADGE =
            new WritableObjectPropertyKey(true);
    public static final WritableObjectPropertyKey<ThemeProperty> THEME =
            new WritableObjectPropertyKey<>(true);
    public static final WritableFloatPropertyKey TRANSLATION_X = new WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                APP_MENU_BUTTON_HELPER,
                CONTENT_DESCRIPTION,
                IS_CLICKABLE,
                IS_HIGHLIGHTING,
                IS_VISIBLE,
                STATE_SUPPLIER,
                SHOW_UPDATE_BADGE,
                THEME,
                TRANSLATION_X
            };
}
