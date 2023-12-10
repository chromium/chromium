// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties.AUTOFILL_PROFILE;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties.ON_CLICK_LISTENER;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder class for Autofill profile items on the detail sheet. */
class AutofillProfileItemViewBinder {
    /** Creates a view for Autofill profile items on the detail sheet. */
    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.fast_checkout_autofill_profile_item, parent, false);
    }

    /** Binds the item view with to the model properties. */
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AUTOFILL_PROFILE) {
            FastCheckoutAutofillProfile profile = model.get(AUTOFILL_PROFILE);
            TextView fullNameView =
                    view.findViewById(R.id.fast_checkout_autofill_profile_item_name);
            fullNameView.setText(profile.getFullName());

            TextView streetAddressView =
                    view.findViewById(R.id.fast_checkout_autofill_profile_item_street_address);
            streetAddressView.setText(profile.getStreetAddress());

            TextView postalCodeView =
                    view.findViewById(
                            R.id.fast_checkout_autofill_profile_item_city_and_postal_code);
            postalCodeView.setText(getLocalityAndPostalCode(profile));

            TextView countryView =
                    view.findViewById(R.id.fast_checkout_autofill_profile_item_country);
            countryView.setText(profile.getCountryName());

            TextView emailView = view.findViewById(R.id.fast_checkout_autofill_profile_item_email);
            emailView.setText(profile.getEmailAddress());

            TextView phoneNumber =
                    view.findViewById(R.id.fast_checkout_autofill_profile_item_phone_number);
            phoneNumber.setText(profile.getPhoneNumber());

            hideIfEmpty(fullNameView);
            hideIfEmpty(streetAddressView);
            hideIfEmpty(postalCodeView);
            hideIfEmpty(countryView);
            hideIfEmpty(emailView);
            hideIfEmpty(phoneNumber);

            // Hide address profile subsection if empty.
            View profileSubSectionView =
                    view.findViewById(R.id.fast_checkout_autofill_profile_sub_section);
            profileSubSectionView.setVisibility(
                    profile.getEmailAddress().isEmpty() && profile.getPhoneNumber().isEmpty()
                            ? View.GONE
                            : View.VISIBLE);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            view.findViewById(R.id.fast_checkout_autofill_profile_item_selected_icon)
                    .setVisibility(model.get(IS_SELECTED) ? View.VISIBLE : View.GONE);
        }
        setAccessibilityContent(view, model.get(AUTOFILL_PROFILE), model.get(IS_SELECTED));
    }

    /**
     * Returns the properly formatted combination of city and postal code. For now,
     * that means adhering to US formatting.
     */
    private static String getLocalityAndPostalCode(FastCheckoutAutofillProfile profile) {
        StringBuilder builder = new StringBuilder();
        builder.append(profile.getLocality());
        // Add divider only if both elements exist.
        if (!profile.getLocality().isEmpty() && !profile.getPostalCode().isEmpty()) {
            builder.append(", ");
        }
        builder.append(profile.getPostalCode());
        return builder.toString();
    }

    private static void setAccessibilityContent(
            View view, FastCheckoutAutofillProfile profile, boolean isSelected) {
        StringBuilder builder = new StringBuilder();
        builder.append(getIfNotEmpty(profile.getFullName()));
        builder.append(getIfNotEmpty(profile.getStreetAddress()));
        builder.append(getIfNotEmpty(getLocalityAndPostalCode(profile)));
        builder.append(getIfNotEmpty(profile.getCountryName()));
        builder.append(getIfNotEmpty(profile.getEmailAddress()));
        builder.append(getIfNotEmpty(profile.getPhoneNumber()));
        builder.append(
                view.getContext()
                        .getResources()
                        .getString(
                                isSelected
                                        ? R.string.fast_checkout_detail_screen_selected_description
                                        : R.string
                                                .fast_checkout_detail_screen_non_selected_description));
        view.setContentDescription(builder.toString());
    }

    private static String getIfNotEmpty(String text) {
        if (!text.isEmpty()) {
            return text + ",";
        }
        return "";
    }

    private static void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }
}
