// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.home_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.SELECTED_CREDIT_CARD;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.SELECTED_PROFILE;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator.Delegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Fast Checkout home screen view. These
 * updates are pulled from the {@link FastCheckoutProperties} when a notification of an update is
 * received.
 */
public class HomeScreenViewBinder {
    static class ViewHolder {
        final Context mContext;
        final TextView mFullNameTextView;
        final TextView mStreetAddressTextView;
        final LinearLayout mProfileSubsectionView;
        final TextView mEmailAddressTextView;
        final TextView mPhoneNumberTextView;
        final TextView mCreditCardHeaderTextView;
        final ImageView mCreditCardImageView;
        final LinearLayout mSelectedAddressView;
        final LinearLayout mSelectedCreditCardView;
        final ButtonCompat mAcceptButton;
        final ImageView mGPayImageView;

        ViewHolder(Context context, View contentView) {
            mContext = context;
            mFullNameTextView =
                    contentView.findViewById(R.id.fast_checkout_home_sheet_profile_name);
            mStreetAddressTextView =
                    contentView.findViewById(R.id.fast_checkout_home_sheet_profile_street);
            mProfileSubsectionView =
                    contentView.findViewById(R.id.fast_checkout_home_sheet_profile_sub_section);
            mEmailAddressTextView =
                    contentView.findViewById(R.id.fast_checkout_home_sheet_profile_email);
            mPhoneNumberTextView =
                    contentView.findViewById(R.id.fast_checkout_home_sheet_profile_phone_number);
            mCreditCardHeaderTextView =
                    contentView.findViewById(R.id.fast_checkout_sheet_selected_credit_card_header);
            mCreditCardImageView = contentView.findViewById(R.id.fast_checkout_credit_card_icon);
            mSelectedAddressView = contentView.findViewById(R.id.selected_address_profile_view);
            mSelectedCreditCardView = contentView.findViewById(R.id.selected_credit_card_view);
            mAcceptButton = contentView.findViewById(R.id.fast_checkout_button_accept);
            mGPayImageView = contentView.findViewById(R.id.fast_checkout_gpay_icon);
        }
    }

    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == HOME_SCREEN_DELEGATE) {
            Delegate delegate = model.get(HOME_SCREEN_DELEGATE);

            view.mSelectedAddressView.setOnClickListener((v) -> delegate.onShowAddressesList());

            view.mSelectedCreditCardView.setOnClickListener((v) -> delegate.onShowCreditCardList());

            view.mAcceptButton.setOnClickListener(
                    (v) -> {
                        view.mAcceptButton.announceForAccessibility(
                                view.mContext
                                        .getResources()
                                        .getString(
                                                R.string
                                                        .fast_checkout_home_sheet_accept_button_clicked_description));
                        delegate.onOptionsAccepted();
                    });
        } else if (propertyKey == SELECTED_PROFILE) {
            updateProfile(model, view);
        } else if (propertyKey == SELECTED_CREDIT_CARD) {
            updateCreditCard(model, view);
        }
    }

    private static String getFullStreetAddress(FastCheckoutAutofillProfile profile) {
        StringBuilder builder = new StringBuilder();
        builder.append(profile.getStreetAddress());
        // Add divider only if both elements exist.
        if (!profile.getStreetAddress().isEmpty() && !profile.getPostalCode().isEmpty()) {
            builder.append(", ");
        }
        builder.append(profile.getPostalCode());
        return builder.toString();
    }

    private static void updateProfile(PropertyModel model, ViewHolder view) {
        FastCheckoutAutofillProfile profile = model.get(SELECTED_PROFILE);
        view.mFullNameTextView.setText(profile.getFullName());
        view.mStreetAddressTextView.setText(getFullStreetAddress(profile));
        view.mEmailAddressTextView.setText(profile.getEmailAddress());
        view.mPhoneNumberTextView.setText(profile.getPhoneNumber());
        hideIfEmpty(view.mFullNameTextView);
        hideIfEmpty(view.mStreetAddressTextView);
        hideIfEmpty(view.mEmailAddressTextView);
        hideIfEmpty(view.mPhoneNumberTextView);

        // Hide address profile subsection if empty.
        view.mProfileSubsectionView.setVisibility(
                profile.getEmailAddress().isEmpty() && profile.getPhoneNumber().isEmpty()
                        ? View.GONE
                        : View.VISIBLE);
    }

    private static void updateCreditCard(PropertyModel model, ViewHolder view) {
        FastCheckoutCreditCard creditCard = model.get(SELECTED_CREDIT_CARD);
        view.mCreditCardHeaderTextView.setText(creditCard.getObfuscatedNumber());
        try {
            view.mCreditCardImageView.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.mContext, creditCard.getIssuerIconDrawableId()));
        } catch (Resources.NotFoundException e) {
            view.mCreditCardImageView.setImageDrawable(null);
        }
        if (creditCard.getIsLocal()) {
            view.mGPayImageView.setVisibility(View.GONE);
        } else {
            view.mGPayImageView.setVisibility(View.VISIBLE);
        }
    }

    private static void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }
}
