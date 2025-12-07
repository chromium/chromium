// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.view.View;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The view binder of the SecurePaymentConfirmation UI, which is stateless. It is called to bind a
 * given model to a given view. Should contain as little business logic as possible.
 */
@NullMarked
/* package */ class SecurePaymentConfirmationViewBinder {
    /* package */ static void bind(
            PropertyModel model, SecurePaymentConfirmationView view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationProperties.HEADER_LOGOS == propertyKey) {
            List<PaymentEntityLogo> logos =
                    model.get(SecurePaymentConfirmationProperties.HEADER_LOGOS);
            if (logos.isEmpty()) {
                view.mHeaderIllustration.setVisibility(View.VISIBLE);
                view.mHeaderLogosRow.setVisibility(View.GONE);
            } else {
                view.mHeaderLogoPrimary.setImageBitmap(logos.get(0).getIcon());
                view.mHeaderLogoPrimary.setContentDescription(logos.get(0).getLabel());
                view.mHeaderLogoPrimary.setScaleType(ScaleType.FIT_CENTER);
                if (logos.size() > 1) {
                    // Note that even if the merchant provides more than 2 logos, SPCAppFactory
                    // should be filtering the logos down to a max size of 2.
                    assert (logos.size() == 2);
                    view.mHeaderLogoSecondary.setImageBitmap(logos.get(1).getIcon());
                    view.mHeaderLogoSecondary.setContentDescription(logos.get(1).getLabel());
                    view.mHeaderLogosDivider.setVisibility(View.VISIBLE);
                    view.mHeaderLogoPrimary.setScaleType(ScaleType.FIT_END);
                } else {
                    view.mHeaderLogosDivider.setVisibility(View.GONE);
                    view.mHeaderLogoSecondary.setVisibility(View.GONE);
                    view.mHeaderLogoPrimary.setScaleType(ScaleType.FIT_CENTER);
                }

                view.mHeaderLogosRow.setVisibility(View.VISIBLE);
                view.mHeaderIllustration.setVisibility(View.GONE);
            }
        } else if (SecurePaymentConfirmationProperties.TITLE == propertyKey) {
            view.mTitle.setText(model.get(SecurePaymentConfirmationProperties.TITLE));
        } else if (SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER == propertyKey) {
            view.mItemList.swapAdapter(
                    model.get(SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER),
                    /* removeAndRecycleExistingViews= */ true);
        } else if (SecurePaymentConfirmationProperties.OPT_OUT_TEXT == propertyKey) {
            if (model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT) == null) {
                view.mOptOutText.setVisibility(View.GONE);
            } else {
                view.mOptOutText.setVisibility(View.VISIBLE);
                view.mOptOutText.setText(
                        model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
            }
        } else if (SecurePaymentConfirmationProperties.FOOTNOTE == propertyKey) {
            if (model.get(SecurePaymentConfirmationProperties.FOOTNOTE) == null) {
                view.mFootnote.setVisibility(View.GONE);
            } else {
                view.mFootnote.setVisibility(View.VISIBLE);
                view.mFootnote.setText(model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
            }
        } else if (SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL == propertyKey) {
            view.mContinueButton.setText(
                    model.get(SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL));
        }
    }

    static void bindItem(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationProperties.ItemProperties.ICON == propertyKey) {
            ImageView iconView = view.findViewById(R.id.icon);
            iconView.setImageDrawable(
                    model.get(SecurePaymentConfirmationProperties.ItemProperties.ICON));
        } else if (SecurePaymentConfirmationProperties.ItemProperties.ICON_LABEL == propertyKey) {
            ImageView iconView = view.findViewById(R.id.icon);
            iconView.setContentDescription(
                    model.get(SecurePaymentConfirmationProperties.ItemProperties.ICON_LABEL));
        } else if (SecurePaymentConfirmationProperties.ItemProperties.PRIMARY_TEXT == propertyKey) {
            TextView primaryTextView = view.findViewById(R.id.primary_text);
            primaryTextView.setText(
                    model.get(SecurePaymentConfirmationProperties.ItemProperties.PRIMARY_TEXT));
        } else if (SecurePaymentConfirmationProperties.ItemProperties.SECONDARY_TEXT
                == propertyKey) {
            TextView secondaryTextView = view.findViewById(R.id.secondary_text);
            if (model.get(SecurePaymentConfirmationProperties.ItemProperties.SECONDARY_TEXT) == null
                    || model.get(SecurePaymentConfirmationProperties.ItemProperties.SECONDARY_TEXT)
                            .isEmpty()) {
                secondaryTextView.setVisibility(View.GONE);
            } else {
                secondaryTextView.setText(
                        model.get(
                                SecurePaymentConfirmationProperties.ItemProperties.SECONDARY_TEXT));
                secondaryTextView.setVisibility(View.VISIBLE);
            }
        }
    }
}
