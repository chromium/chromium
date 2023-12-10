// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties.CREDIT_CARD;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties.ON_CLICK_LISTENER;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder class for credit card items on the detail sheet. */
class CreditCardItemViewBinder {
    /** Creates a view for credit card items on the detail sheet. */
    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.fast_checkout_credit_card_item, parent, false);
    }

    /** Binds the item view to the model properties. */
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == CREDIT_CARD) {
            FastCheckoutCreditCard card = model.get(CREDIT_CARD);

            TextView numberView = view.findViewById(R.id.fast_checkout_credit_card_item_number);
            numberView.setText(card.getObfuscatedNumber());

            TextView nameView = view.findViewById(R.id.fast_checkout_credit_card_item_name);
            nameView.setText(card.getName());

            TextView expirationDateViewView =
                    view.findViewById(R.id.fast_checkout_credit_card_item_expiration_date);
            expirationDateViewView.setText(card.getFormattedExpirationDate(view.getContext()));

            ImageView icon = view.findViewById(R.id.fast_checkout_credit_card_icon);

            hideIfEmpty(numberView);
            hideIfEmpty(nameView);
            hideIfEmpty(expirationDateViewView);

            try {
                icon.setImageDrawable(
                        AppCompatResources.getDrawable(
                                icon.getContext(), card.getIssuerIconDrawableId()));
            } catch (Resources.NotFoundException e) {
                icon.setImageDrawable(null);
            }
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            view.findViewById(R.id.fast_checkout_credit_card_item_selected_icon)
                    .setVisibility(model.get(IS_SELECTED) ? View.VISIBLE : View.GONE);
        }
        setAccessibilityContent(view, model.get(CREDIT_CARD), model.get(IS_SELECTED));
    }

    private static void setAccessibilityContent(
            View view, FastCheckoutCreditCard card, boolean isSelected) {
        StringBuilder builder = new StringBuilder();
        builder.append(getIfNotEmpty(card.getObfuscatedNumber()));
        builder.append(getIfNotEmpty(card.getName()));
        String expiryDateString = card.getFormattedExpirationDate(view.getContext());
        if (!expiryDateString.isEmpty()) {
            builder.append(
                    view.getContext()
                            .getResources()
                            .getString(R.string.fast_checkout_credit_card_item_expire_description));
            builder.append(getIfNotEmpty(" " + expiryDateString));
        }
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
