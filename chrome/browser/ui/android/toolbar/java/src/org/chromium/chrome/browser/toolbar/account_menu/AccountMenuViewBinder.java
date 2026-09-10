// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.IdentityCardProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.PromoCardProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Account Menu popup items. */
@NullMarked
public class AccountMenuViewBinder {
    public static void bindMenuItem(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = (TextView) view;
        if (propertyKey == MenuItemProperties.TITLE_ID) {
            textView.setText(model.get(MenuItemProperties.TITLE_ID));
        } else if (propertyKey == MenuItemProperties.START_ICON_ID) {
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    model.get(MenuItemProperties.START_ICON_ID), 0, 0, 0);
        } else if (propertyKey == MenuItemProperties.CLICK_LISTENER) {
            textView.setOnClickListener(model.get(MenuItemProperties.CLICK_LISTENER));
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }

    public static void bindPromoCard(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == PromoCardProperties.ON_SIGNIN_CLICK_LISTENER) {
            View signinButton = view.findViewById(R.id.account_menu_signin_button);
            signinButton.setOnClickListener(
                    model.get(PromoCardProperties.ON_SIGNIN_CLICK_LISTENER));
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }

    public static void bindIdentityCard(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IdentityCardProperties.PROFILE_DATA) {
            DisplayableProfileData profileData = model.get(IdentityCardProperties.PROFILE_DATA);

            ImageView avatarView = view.findViewById(R.id.account_menu_identity_avatar);
            avatarView.setImageDrawable(profileData.getImage());

            TextView nameView = view.findViewById(R.id.account_menu_identity_name);
            TextView emailView = view.findViewById(R.id.account_menu_identity_email);

            @Nullable String fullName = profileData.getFullName();
            if (!TextUtils.isEmpty(fullName)) {
                nameView.setText(fullName);
                if (profileData.hasDisplayableEmailAddress()) {
                    emailView.setText(profileData.getAccountEmail());
                    emailView.setVisibility(View.VISIBLE);
                } else {
                    emailView.setVisibility(View.GONE);
                }
            } else if (profileData.hasDisplayableEmailAddress()) {
                nameView.setText(profileData.getAccountEmail());
                emailView.setVisibility(View.GONE);
            } else {
                nameView.setText(profileData.getFullNameOrFallbackName(view.getContext()));
                emailView.setVisibility(View.GONE);
            }
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }
}
