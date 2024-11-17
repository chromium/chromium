// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.view.View;

import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** View binder than binds the promo view with the property model. */
class PromoCardViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, PromoCardView, PropertyKey> {
    private static final String TAG = "PromoCardViewBinder";

    @Override
    public void bind(PropertyModel model, PromoCardView view, PropertyKey propertyKey) {
        if (propertyKey == PromoCardProperties.IMAGE) {
            view.mPromoImage.setImageDrawable(model.get(PromoCardProperties.IMAGE));
        } else if (propertyKey == PromoCardProperties.ICON_TINT) {
            ImageViewCompat.setImageTintList(
                    view.mPromoImage, model.get(PromoCardProperties.ICON_TINT));
        } else if (propertyKey == PromoCardProperties.TITLE) {
            view.mTitle.setText(model.get(PromoCardProperties.TITLE));
        } else if (propertyKey == PromoCardProperties.DESCRIPTION) {
            if (view.mDescription == null) {
                Log.w(TAG, "Description does not exist in the promo card.");
                return;
            }
            view.mDescription.setText(model.get(PromoCardProperties.DESCRIPTION));
        } else if (propertyKey == PromoCardProperties.PRIMARY_BUTTON_TEXT) {
            view.mPrimaryButton.setText(model.get(PromoCardProperties.PRIMARY_BUTTON_TEXT));
        } else if (propertyKey == PromoCardProperties.SECONDARY_BUTTON_TEXT) {
            if (view.mSecondaryButton == null) {
                Log.w(TAG, "Description does not exist in the promo card.");
                return;
            }
            view.mSecondaryButton.setText(model.get(PromoCardProperties.SECONDARY_BUTTON_TEXT));
        } else if (propertyKey == PromoCardProperties.BUTTONS_WIDTH) {
            view.mPrimaryButton.getLayoutParams().width =
                    model.get(PromoCardProperties.BUTTONS_WIDTH);
            if (view.mSecondaryButton != null) {
                view.mSecondaryButton.getLayoutParams().width =
                        model.get(PromoCardProperties.BUTTONS_WIDTH);
            }

            // Visibility properties
        } else if (propertyKey == PromoCardProperties.HAS_SECONDARY_BUTTON) {
            if (view.mSecondaryButton == null) {
                Log.w(TAG, "Secondary button does not exist in the promo card.");
                return;
            }
            view.mSecondaryButton.setVisibility(
                    model.get(PromoCardProperties.HAS_SECONDARY_BUTTON) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == PromoCardProperties.HAS_CLOSE_BUTTON) {
            if (view.mCloseButton == null) {
                Log.w(TAG, "Close button does not exist in the promo card.");
                return;
            }
            view.mCloseButton.setVisibility(
                    model.get(PromoCardProperties.HAS_CLOSE_BUTTON) ? View.VISIBLE : View.GONE);

            // Callback properties
        } else if (propertyKey == PromoCardProperties.PRIMARY_BUTTON_CALLBACK) {
            Callback<View> callback = model.get(PromoCardProperties.PRIMARY_BUTTON_CALLBACK);
            view.mPrimaryButton.setOnClickListener(callback::onResult);
        } else if (propertyKey == PromoCardProperties.SECONDARY_BUTTON_CALLBACK) {
            if (view.mSecondaryButton == null) {
                Log.w(TAG, "Secondary button does not exist in the promo card.");
                return;
            }
            Callback<View> callback = model.get(PromoCardProperties.SECONDARY_BUTTON_CALLBACK);
            view.mSecondaryButton.setOnClickListener(callback::onResult);
        } else if (propertyKey == PromoCardProperties.CLOSE_BUTTON_CALLBACK) {
            if (view.mCloseButton == null) {
                Log.w(TAG, "Close button does not exist in the promo card.");
                return;
            }
            Callback<View> callback = model.get(PromoCardProperties.CLOSE_BUTTON_CALLBACK);
            view.mCloseButton.setOnClickListener(callback::onResult);
        }
    }
}
