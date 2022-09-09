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
            ((TextView) view.findViewById(R.id.fast_checkout_autofill_profile_item_name))
                    .setText(profile.getFullName());
            ((TextView) view.findViewById(R.id.fast_checkout_autofill_profile_item_street_address))
                    .setText(profile.getStreetAddress());
            ((TextView) view.findViewById(
                     R.id.fast_checkout_autofill_profile_item_city_and_postal_code))
                    .setText(getCityAndPostalCode(profile));
            ((TextView) view.findViewById(R.id.fast_checkout_autofill_profile_item_country))
                    .setText(profile.getCountryName());
            ((TextView) view.findViewById(R.id.fast_checkout_autofill_profile_item_email))
                    .setText(profile.getEmailAddress());
            ((TextView) view.findViewById(R.id.fast_checkout_autofill_profile_item_phone_number))
                    .setText(profile.getPhoneNumber());
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            view.findViewById(R.id.fast_checkout_autofill_profile_item_selected_icon)
                    .setVisibility(model.get(IS_SELECTED) ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Returns the properly formatted combination of city and postal code. For now,
     * that means adhering to US formatting.
     */
    private static String getCityAndPostalCode(FastCheckoutAutofillProfile profile) {
        StringBuilder builder = new StringBuilder();
        builder.append(profile.getLocality());
        builder.append(", ");
        builder.append(profile.getPostalCode());
        return builder.toString();
    }
}
