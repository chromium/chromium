// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.IdentityCardProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.PromoCardProperties;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.containment.ContainerStyle;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Account Menu popup items. */
@NullMarked
public class AccountMenuViewBinder {
    public static void bindMenuItem(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = (TextView) view;
        if (propertyKey == MenuItemProperties.TITLE_ID) {
            textView.setText(model.get(MenuItemProperties.TITLE_ID));
        } else if (propertyKey == MenuItemProperties.START_ICON_ID
                || propertyKey == MenuItemProperties.SHOW_ICON_BADGE) {
            boolean showIconBadge = model.get(MenuItemProperties.SHOW_ICON_BADGE);
            int iconId = model.get(MenuItemProperties.START_ICON_ID);
            if (showIconBadge) {
                Drawable icon = AppCompatResources.getDrawable(textView.getContext(), iconId);
                assert icon != null;
                Drawable badgedIcon =
                        UiUtils.drawIconWithBadge(
                                textView.getContext(),
                                icon,
                                R.color.default_icon_color_tint_list,
                                R.dimen.account_menu_icon_badge_size,
                                R.dimen.account_menu_icon_badge_border_size,
                                R.color.default_red);
                textView.setCompoundDrawableTintList(null);
                textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                        badgedIcon, null, null, null);
            } else {
                textView.setCompoundDrawableTintList(
                        AppCompatResources.getColorStateList(
                                textView.getContext(), R.color.default_icon_color_tint_list));
                textView.setCompoundDrawablesRelativeWithIntrinsicBounds(iconId, 0, 0, 0);
            }
        } else if (propertyKey == MenuItemProperties.CLICK_LISTENER) {
            textView.setOnClickListener(model.get(MenuItemProperties.CLICK_LISTENER));
        } else if (propertyKey == MenuItemProperties.IS_SECTION_TOP
                || propertyKey == MenuItemProperties.IS_SECTION_BOTTOM) {
            Context context = view.getContext();
            if (SigninFeatureMap.isEnabled(SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU_REFINEMENTS)) {
                ContainerStyle style =
                        new ContainmentItemController(context)
                                .createStandardBuilder(
                                        model.get(MenuItemProperties.IS_SECTION_TOP),
                                        model.get(MenuItemProperties.IS_SECTION_BOTTOM),
                                        /* isSingleLine= */ true)
                                .build();
                ContainmentViewStyler.applyBackgroundStyle(view, style);
                ContainmentViewStyler.applyMargins(view, style);
            } else {
                // TODO(crbug.com/565733817): Remove when SigninButtonProfileMenuRefinements is
                // launched.
                int horizontalPadding =
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.account_menu_horizontal_padding_legacy);
                view.setPaddingRelative(
                        horizontalPadding,
                        view.getPaddingTop(),
                        horizontalPadding,
                        view.getPaddingBottom());
            }
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
        } else if (propertyKey == IdentityCardProperties.SHOULD_DISPLAY_MANAGED_HEADER) {
            boolean shouldDisplayManagedHeader =
                    model.get(IdentityCardProperties.SHOULD_DISPLAY_MANAGED_HEADER);
            View managedHeader = view.findViewById(R.id.account_menu_managed_header);
            managedHeader.setVisibility(shouldDisplayManagedHeader ? View.VISIBLE : View.GONE);
            if (shouldDisplayManagedHeader) {
                Context context = view.getContext();
                ContainerStyle style =
                        new ContainmentItemController(context)
                                .createStandardBuilder(
                                        /* isTop= */ true,
                                        /* isBottom= */ true,
                                        /* isSingleLine= */ true)
                                .setBackgroundColor(SemanticColorUtils.getColorSurfaceDim(context))
                                .build();
                ContainmentViewStyler.applyBackgroundStyle(managedHeader, style);
            }
        } else if (propertyKey == IdentityCardProperties.MANAGED_HEADER_CLICK_LISTENER) {
            view.findViewById(R.id.account_menu_managed_header)
                    .setOnClickListener(
                            model.get(IdentityCardProperties.MANAGED_HEADER_CLICK_LISTENER));
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }
}
