// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.text.method.LinkMovementMethod;
import android.transition.TransitionManager;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.ExistingAccountRowViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Stateless FREBottomGroup view binder.
 * TODO(crbug/1248083): Create a customised view class for the view here
 */
class SigninFirstRunViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SigninFirstRunProperties.ON_CONTINUE_AS_CLICKED) {
            view.findViewById(R.id.signin_fre_continue_button)
                    .setOnClickListener(model.get(SigninFirstRunProperties.ON_CONTINUE_AS_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.ON_DISMISS_CLICKED) {
            view.findViewById(R.id.signin_fre_dismiss_button)
                    .setOnClickListener(model.get(SigninFirstRunProperties.ON_DISMISS_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.findViewById(R.id.signin_fre_selected_account)
                    .setOnClickListener(
                            model.get(SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) {
            updateSelectedAccount(view, model);
        } else if (propertyKey == SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED) {
            final boolean isSelectedAccountSupervised =
                    model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
            view.findViewById(R.id.signin_fre_selected_account)
                    .setEnabled(!isSelectedAccountSupervised);
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED) {
            // Add a transition animation between the view changes.
            final ViewGroup freContent = (ViewGroup) view.findViewById(R.id.signin_fre_content);
            TransitionManager.beginDelayedTransition(freContent);
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.FRE_POLICY) {
            view.findViewById(R.id.fre_browser_managed_by_organization)
                    .setVisibility(model.get(SigninFirstRunProperties.FRE_POLICY) != null
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (propertyKey == SigninFirstRunProperties.IS_SIGNIN_SUPPORTED) {
            if (!model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
                ButtonCompat button = view.findViewById(R.id.signin_fre_continue_button);
                button.setText(R.string.continue_button);
                updateVisibility(view, model);
            }
        } else if (propertyKey == SigninFirstRunProperties.FOOTER_STRING) {
            final TextView footerView = view.findViewById(R.id.signin_fre_footer);
            footerView.setText(model.get(SigninFirstRunProperties.FOOTER_STRING));
            footerView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            throw new IllegalArgumentException("Unknown property key:" + propertyKey);
        }
    }

    private static void updateSelectedAccount(View view, PropertyModel model) {
        if (!model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            return;
        }
        final @Nullable DisplayableProfileData profileData =
                model.get(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA);
        final ButtonCompat continueButton = view.findViewById(R.id.signin_fre_continue_button);
        if (profileData == null) {
            continueButton.setText(R.string.signin_add_account_to_device);
        } else {
            ExistingAccountRowViewBinder.bindAccountView(
                    profileData, view.findViewById(R.id.signin_fre_selected_account));
            continueButton.setText(view.getContext().getString(R.string.signin_promo_continue_as,
                    profileData.getGivenNameOrFullNameOrEmail()));
        }
        updateVisibility(view, model);
    }

    private static void updateVisibility(View view, PropertyModel model) {
        final boolean areNativeAndPolicyLoaded =
                model.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED);
        view.findViewById(R.id.signin_fre_progress_spinner)
                .setVisibility(areNativeAndPolicyLoaded ? View.GONE : View.VISIBLE);

        final int selectedAccountVisibility = areNativeAndPolicyLoaded
                        && model.get(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) != null
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                ? View.VISIBLE
                : View.GONE;
        view.findViewById(R.id.signin_fre_selected_account)
                .setVisibility(selectedAccountVisibility);
        final boolean isSelectedAccountSupervised =
                model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
        view.findViewById(R.id.signin_fre_selected_account_expand_icon)
                .setVisibility(
                        selectedAccountVisibility == View.VISIBLE && isSelectedAccountSupervised
                                ? View.INVISIBLE
                                : View.VISIBLE);
        final int dismissButtonVisibility = areNativeAndPolicyLoaded
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                        && !isSelectedAccountSupervised
                ? View.VISIBLE
                : View.GONE;
        view.findViewById(R.id.signin_fre_dismiss_button).setVisibility(dismissButtonVisibility);

        final int otherElementsVisibility = areNativeAndPolicyLoaded ? View.VISIBLE : View.GONE;
        view.findViewById(R.id.signin_fre_continue_button).setVisibility(otherElementsVisibility);
        view.findViewById(R.id.signin_fre_footer).setVisibility(otherElementsVisibility);
    }

    private SigninFirstRunViewBinder() {}
}
