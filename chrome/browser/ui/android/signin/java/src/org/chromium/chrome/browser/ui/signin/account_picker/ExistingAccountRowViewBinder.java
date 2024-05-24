// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * This class regroups the buildView and bindView util methods of the
 * existing account row.
 */
public class ExistingAccountRowViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    /**
     * View binder that associates an existing account view with the model of
     * {@link ExistingAccountRowProperties}.
     */
    @Override
    public void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        boolean isCurrentlySelected = model.get(ExistingAccountRowProperties.IS_CURRENTLY_SELECTED);
        if (propertyKey == ExistingAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(
                    v ->
                            model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER)
                                    .onResult(profileData));
        } else if ((propertyKey == ExistingAccountRowProperties.PROFILE_DATA)
                || (propertyKey == ExistingAccountRowProperties.IS_CURRENTLY_SELECTED)) {
            bindAccountView(profileData, view, isCurrentlySelected);
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }

    private static void setAccountTextPrimary(
            DisplayableProfileData profileData, View accountView) {
        TextView accountTextPrimary = accountView.findViewById(R.id.account_text_primary);
        if (!TextUtils.isEmpty(profileData.getFullName())) {
            accountTextPrimary.setText(profileData.getFullName());
        } else if (!profileData.hasDisplayableEmailAddress()) {
            // Cannot display the email address and empty full name; use default account string.
            accountTextPrimary.setText(R.string.default_google_account_username);
        } else {
            // Full name is not available, show the email address.
            accountTextPrimary.setText(profileData.getAccountEmail());
        }
    }

    private static void setAccountTextSecondary(
            DisplayableProfileData profileData, View accountView) {
        TextView accountTextSecondary = accountView.findViewById(R.id.account_text_secondary);
        if (!profileData.hasDisplayableEmailAddress()) {
            // If the email address cannot be displayed, the primary TextView either displays the
            // full name or the default account string. The secondary TextView is hidden.
            accountTextSecondary.setVisibility(View.GONE);
        } else {
            // If the full name is available, the email will be in the secondary TextView.
            // Otherwise, the email is in the primary TextView; the secondary TextView hidden.
            final int secondaryTextVisibility =
                    TextUtils.isEmpty(profileData.getFullName()) ? View.GONE : View.VISIBLE;
            if (secondaryTextVisibility == View.VISIBLE) {
                accountTextSecondary.setText(profileData.getAccountEmail());
            }
            accountTextSecondary.setVisibility(secondaryTextVisibility);
        }
    }

    /**
     * Binds the view with the given profile data.
     *
     * @param profileData profile data needs to bind.
     * @param view A view object inflated from R.layout.account_picker_row,
     *     R.layout.account_picker_dialog_row or R.layout.account_picker_bottom_sheet_row.
     * @param isCurrentlySelected whether the account is the one which is currently selected by the
     *     user.
     */
    public static void bindAccountView(
            DisplayableProfileData profileData, View view, boolean isCurrentlySelected) {
        ImageView accountImage = view.findViewById(R.id.account_image);
        accountImage.setImageDrawable(profileData.getImage());
        setAccountTextPrimary(profileData, view);
        setAccountTextSecondary(profileData, view);
        view.setContentDescription(
                SigninUtils.getChooseAccountLabel(
                        view.getContext(), profileData, isCurrentlySelected));
    }
}
