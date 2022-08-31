// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.fast_checkout.R;

/** A ViewHolder for an individual Autofill profile item entry. */
class AutofillProfileItemViewHolder extends RecyclerView.ViewHolder {
    // Text fields.
    public final TextView name;
    public final TextView streetAddress;
    public final TextView cityAndPostalCode;
    public final TextView country;
    public final TextView email;
    public final TextView phoneNumber;

    // The check icon that indicates whether the profile is the selected one.
    public final ImageView selectedIcon;

    AutofillProfileItemViewHolder(ViewGroup parent, @LayoutRes int layout) {
        super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));

        name = itemView.findViewById(R.id.fast_checkout_autofill_profile_item_name);
        streetAddress =
                itemView.findViewById(R.id.fast_checkout_autofill_profile_item_street_address);
        cityAndPostalCode = itemView.findViewById(
                R.id.fast_checkout_autofill_profile_item_city_and_postal_code);
        country = itemView.findViewById(R.id.fast_checkout_autofill_profile_item_country);
        email = itemView.findViewById(R.id.fast_checkout_autofill_profile_item_email);
        phoneNumber = itemView.findViewById(R.id.fast_checkout_autofill_profile_item_phone_number);

        selectedIcon =
                itemView.findViewById(R.id.fast_checkout_autofill_profile_item_selected_icon);
    }
}
