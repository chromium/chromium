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
        if (propertyKey == ExistingAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(v
                    -> model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER)
                               .onResult(profileData));
        } else if (propertyKey == ExistingAccountRowProperties.PROFILE_DATA) {
            bindAccountView(profileData, view);
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }

    /**
     * Binds the view with the given profile data.
     *
     * @param profileData profile data needs to bind.
     * @param view A view object inflated from @layout/account_picker_row.
     */
    public static void bindAccountView(DisplayableProfileData profileData, View view) {
        ImageView accountImage = view.findViewById(R.id.account_image);
        accountImage.setImageDrawable(profileData.getImage());

        TextView accountTextPrimary = view.findViewById(R.id.account_text_primary);
        TextView accountTextSecondary = view.findViewById(R.id.account_text_secondary);

        String fullName = profileData.getFullName();
        if (!TextUtils.isEmpty(fullName)) {
            accountTextPrimary.setText(fullName);
            accountTextSecondary.setText(profileData.getAccountEmail());
            accountTextSecondary.setVisibility(View.VISIBLE);
        } else {
            // Full name is not available, show the email in the primary TextView.
            accountTextPrimary.setText(profileData.getAccountEmail());
            accountTextSecondary.setVisibility(View.GONE);
        }
    }
}
