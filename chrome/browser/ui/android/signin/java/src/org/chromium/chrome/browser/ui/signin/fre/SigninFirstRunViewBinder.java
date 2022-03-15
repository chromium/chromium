// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.text.method.LinkMovementMethod;
import android.transition.TransitionManager;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.ExistingAccountRowViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless SigninFirstRun view binder.
 */
class SigninFirstRunViewBinder {
    static void bind(PropertyModel model, SigninFirstRunView view, PropertyKey propertyKey) {
        if (propertyKey == SigninFirstRunProperties.ON_CONTINUE_AS_CLICKED) {
            view.getContinueButtonView().setOnClickListener(
                    model.get(SigninFirstRunProperties.ON_CONTINUE_AS_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.ON_DISMISS_CLICKED) {
            view.getDismissButtonView().setOnClickListener(
                    model.get(SigninFirstRunProperties.ON_DISMISS_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.IS_CONTINUE_OR_DISMISS_CLICKED) {
            // TODO(https://crbug.com/1294994): Add a spinner while waiting for sign(in/out)
            //  to finish.
        } else if (propertyKey == SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView().setOnClickListener(
                    model.get(SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) {
            updateSelectedAccount(view, model);
        } else if (propertyKey == SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED) {
            final boolean isSelectedAccountSupervised =
                    model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
            view.getSelectedAccountView().setEnabled(!isSelectedAccountSupervised);
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED) {
            // Add a transition animation between the view changes.
            TransitionManager.beginDelayedTransition(view.getContentView());
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.FRE_POLICY) {
            view.getBrowserManagedHeaderView().setVisibility(
                    model.get(SigninFirstRunProperties.FRE_POLICY) != null ? View.VISIBLE
                                                                           : View.GONE);
        } else if (propertyKey == SigninFirstRunProperties.IS_SIGNIN_SUPPORTED) {
            if (!model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
                view.getContinueButtonView().setText(R.string.continue_button);
                updateVisibility(view, model);
            }
        } else if (propertyKey == SigninFirstRunProperties.FOOTER_STRING) {
            view.getFooterView().setText(model.get(SigninFirstRunProperties.FOOTER_STRING));
            view.getFooterView().setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            throw new IllegalArgumentException("Unknown property key:" + propertyKey);
        }
    }

    private static void updateSelectedAccount(SigninFirstRunView view, PropertyModel model) {
        if (!model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            return;
        }
        final @Nullable DisplayableProfileData profileData =
                model.get(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA);
        if (profileData == null) {
            view.getContinueButtonView().setText(R.string.signin_add_account_to_device);
        } else {
            ExistingAccountRowViewBinder.bindAccountView(
                    profileData, view.getSelectedAccountView());
            view.getContinueButtonView().setText(
                    view.getContext().getString(R.string.signin_promo_continue_as,
                            profileData.getGivenNameOrFullNameOrEmail()));
        }
        updateVisibility(view, model);
    }

    private static void updateVisibility(SigninFirstRunView view, PropertyModel model) {
        final boolean areNativeAndPolicyLoaded =
                model.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED);
        view.getProgressSpinnerView().setVisibility(
                areNativeAndPolicyLoaded ? View.GONE : View.VISIBLE);
        if (areNativeAndPolicyLoaded) view.onNativeAndPoliciesLoaded();

        final int selectedAccountVisibility = areNativeAndPolicyLoaded
                        && model.get(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) != null
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                ? View.VISIBLE
                : View.GONE;
        view.getSelectedAccountView().setVisibility(selectedAccountVisibility);
        final boolean isSelectedAccountSupervised =
                model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
        view.getExpandIconView().setVisibility(
                selectedAccountVisibility == View.VISIBLE && isSelectedAccountSupervised
                        ? View.INVISIBLE
                        : View.VISIBLE);
        final int dismissButtonVisibility = areNativeAndPolicyLoaded
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                        && !isSelectedAccountSupervised
                ? View.VISIBLE
                : View.GONE;
        view.getDismissButtonView().setVisibility(dismissButtonVisibility);

        final int otherElementsVisibility = areNativeAndPolicyLoaded ? View.VISIBLE : View.GONE;
        view.getContinueButtonView().setVisibility(otherElementsVisibility);
        view.getFooterView().setVisibility(otherElementsVisibility);
    }

    private SigninFirstRunViewBinder() {}
}
