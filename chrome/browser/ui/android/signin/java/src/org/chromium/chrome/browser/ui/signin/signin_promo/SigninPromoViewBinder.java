// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

@NullMarked
final class SigninPromoViewBinder {
    public static void bind(
            PropertyModel model, PersonalizedSigninPromoView view, PropertyKey key) {
        Context context = view.getContext();
        @SigninFeatureMap.SeamlessSigninPromoType
        int seamlessSigninPromoType = SigninFeatureMap.getInstance().getSeamlessSigninPromoType();
        if (key == SigninPromoProperties.PROFILE_DATA) {
            DisplayableProfileData profileData = model.get(SigninPromoProperties.PROFILE_DATA);
            if (profileData == null) {
                if (seamlessSigninPromoType != SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                    view.getImage().setImageResource(R.drawable.chrome_sync_logo);
                    int imageDim =
                            seamlessSigninPromoType
                                            == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS
                                    ? R.dimen.seamless_signin_promo_cold_state_image_size
                                    : R.dimen.signin_promo_cold_state_image_size;
                    // TODO(crbug.com/456378546): move this logic to SigninPromoCoordinator
                    setImageSize(context, view, imageDim);
                }
            } else {
                Drawable accountImage = profileData.getImage();
                view.getImage().setImageDrawable(accountImage);
                // TODO(crbug.com/456378546): move this logic to SigninPromoCoordinator
                int imageDim =
                        seamlessSigninPromoType == SigninFeatureMap.SeamlessSigninPromoType.COMPACT
                                ? R.dimen.seamless_signin_promo_account_image_size_compact
                                : R.dimen.sync_promo_account_image_size;
                setImageSize(context, view, imageDim);
                if (seamlessSigninPromoType == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                    TextView accountTextPrimary = view.findViewById(R.id.account_text_primary);
                    TextView accountTextSecondary = view.findViewById(R.id.account_text_secondary);
                    accountTextPrimary.setText(profileData.getFullName());
                    accountTextSecondary.setText(profileData.getAccountEmail());
                    view.getSignedInPromoProfileImage().setImageDrawable(accountImage);
                }
            }
        } else if (key == SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED) {
            view.getPrimaryButton()
                    .setOnClickListener(model.get(SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED));
        } else if (key == SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED) {
            if (seamlessSigninPromoType != SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                view.getSecondaryButton()
                        .setOnClickListener(
                                model.get(SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED));
            } else {
                view.getSelectedAccountView()
                        .setOnClickListener(
                                model.get(SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED));
            }
        } else if (key == SigninPromoProperties.ON_DISMISS_BUTTON_CLICKED) {
            view.getDismissButton()
                    .setOnClickListener(model.get(SigninPromoProperties.ON_DISMISS_BUTTON_CLICKED));
        } else if (key == SigninPromoProperties.TITLE_TEXT) {
            view.getTitle().setText(model.get(SigninPromoProperties.TITLE_TEXT));
            updateDismissButtonContentDescription(view, view.getTitle().getText().toString());
        } else if (key == SigninPromoProperties.DESCRIPTION_TEXT) {
            view.getDescription().setText(model.get(SigninPromoProperties.DESCRIPTION_TEXT));
        } else if (key == SigninPromoProperties.PRIMARY_BUTTON_TEXT) {
            view.getPrimaryButton().setText(model.get(SigninPromoProperties.PRIMARY_BUTTON_TEXT));
        } else if (key == SigninPromoProperties.SECONDARY_BUTTON_TEXT) {
            if (seamlessSigninPromoType != SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                view.getSecondaryButton()
                        .setText(model.get(SigninPromoProperties.SECONDARY_BUTTON_TEXT));
            }
        } else if (key == SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON) {
            if (seamlessSigninPromoType != SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                int secondaryButtonVisibility =
                        model.get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON)
                                ? View.GONE
                                : View.VISIBLE;
                view.getSecondaryButton().setVisibility(secondaryButtonVisibility);
            }
        } else if (key == SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON) {
            // We use View.INVISIBLE instead of View.GONE to ensure that the layout height remains
            // consistent even when the button is hidden.
            int dismissButtonVisibility =
                    model.get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON)
                            ? View.INVISIBLE
                            : View.VISIBLE;
            view.getDismissButton().setVisibility(dismissButtonVisibility);
        } else if (key == SigninPromoProperties.SHOULD_SHOW_HEADER_WITH_AVATAR) {
            showHeaderWithAvatar(
                    model.get(SigninPromoProperties.SHOULD_SHOW_HEADER_WITH_AVATAR),
                    seamlessSigninPromoType,
                    context,
                    view);
        } else if (key == SigninPromoProperties.SHOULD_SHOW_ACCOUNT_PICKER) {
            if (seamlessSigninPromoType == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                int accountPickerVisibility =
                        model.get(SigninPromoProperties.SHOULD_SHOW_ACCOUNT_PICKER)
                                ? View.VISIBLE
                                : View.GONE;
                view.getSelectedAccountView().setVisibility(accountPickerVisibility);
            }
        } else {
            throw new IllegalArgumentException("Unknown property key: " + key);
        }
    }

    private static void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private static void updateDismissButtonContentDescription(
            PersonalizedSigninPromoView view, String promoTitle) {
        // The dismiss button should be read before the other component to keep the traversal order
        // consistent with the visual order for visual TalkBack users. The promo title is added to
        // the button description so the user can understand that the action is tied to the promo
        // view. See
        // https://crbug.com/414444892.
        String dismissButtonDescription =
                promoTitle + " " + view.getContext().getString(R.string.close);
        view.getDismissButton().setContentDescription(dismissButtonDescription);
    }

    private static void updateViewMargins(
            View view,
            @Nullable Integer left,
            @Nullable Integer top,
            @Nullable Integer right,
            @Nullable Integer bottom) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        int leftMargin = left != null ? left : lp.leftMargin;
        int topMargin = top != null ? top : lp.topMargin;
        int rightMargin = right != null ? right : lp.rightMargin;
        int bottomMargin = bottom != null ? bottom : lp.bottomMargin;
        lp.setMargins(leftMargin, topMargin, rightMargin, bottomMargin);
        view.setLayoutParams(lp);
    }

    private static void showHeaderWithAvatar(
            boolean showLayout,
            @SigninFeatureMap.SeamlessSigninPromoType int promoType,
            Context context,
            PersonalizedSigninPromoView view) {
        if (promoType != SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
            return;
        }
        ImageView signedInPromoImage = view.findViewById(R.id.signed_in_promo_image);
        TextView descriptionText = view.findViewById(R.id.signin_promo_description);
        LinearLayout compactLayoutImageAndDescriptionContainer =
                view.getImageAndDescriptionContainer();
        ButtonCompat primaryButton = view.getPrimaryButton();
        if (showLayout) {
            signedInPromoImage.setVisibility(View.VISIBLE);
            int imageAndDescriptionContainerMarginTop =
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.signin_promo_desc_margin_top_signed_in_compact);
            updateViewMargins(
                    compactLayoutImageAndDescriptionContainer,
                    null,
                    imageAndDescriptionContainerMarginTop,
                    null,
                    null);
            descriptionText.setGravity(Gravity.CENTER_VERTICAL);
            int buttonMargins =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.signin_promo_button_margin_two_buttons);
            updateViewMargins(primaryButton, buttonMargins, null, buttonMargins, null);
        } else {
            signedInPromoImage.setVisibility(View.GONE);
            updateViewMargins(compactLayoutImageAndDescriptionContainer, null, 0, null, null);
            descriptionText.setGravity(Gravity.CENTER);
            int buttonMargins =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.signin_promo_button_margin_compact);
            updateViewMargins(primaryButton, buttonMargins, null, buttonMargins, null);
        }
    }
}
