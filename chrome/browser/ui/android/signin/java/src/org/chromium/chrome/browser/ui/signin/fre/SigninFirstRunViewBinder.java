// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.text.method.LinkMovementMethod;
import android.transition.AutoTransition;
import android.transition.TransitionManager;
import android.view.View;
import android.widget.ProgressBar;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
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
        } else if (propertyKey == SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT) {
            updateVisibilityOnButtonClick(view, model);
        } else if (propertyKey == SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER) {
            updateVisibilityOnButtonClick(view, model);
        } else if (propertyKey == SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView().setOnClickListener(
                    model.get(SigninFirstRunProperties.ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) {
            updateSelectedAccount(view, model);
        } else if (propertyKey == SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED) {
            final boolean isSelectedAccountSupervised =
                    model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
            view.getSelectedAccountView().setEnabled(!isSelectedAccountSupervised);

            updateBrowserManagedHeaderView(view, model);
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER) {
            final boolean showInitialLoadProgressSpinner =
                    model.get(SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER);
            final ProgressBar initialLoadProgressSpinner = view.getInitialLoadProgressSpinnerView();
            if (showInitialLoadProgressSpinner) {
                // The progress spinner is shown at the beginning when layout inflation may not be
                // complete. So it is not possible to use TransitionManager with a startDelay in
                // this case.
                initialLoadProgressSpinner.animate().alpha(1.0f).setStartDelay(500);
            } else {
                TransitionManager.beginDelayedTransition(view);
                initialLoadProgressSpinner.setVisibility(View.GONE);
            }
            updateVisibility(view, model);
        } else if (propertyKey == SigninFirstRunProperties.FRE_POLICY) {
            updateBrowserManagedHeaderView(view, model);
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

    private static void updateBrowserManagedHeaderView(
            SigninFirstRunView view, PropertyModel model) {
        // Supervised accounts do not have any enterprise policy set, but they set app
        // restrictions which the policy load listener considers as policy. But if child
        // accounts are loaded dynamically, policy load listener may say there are no
        // policies on device. Because of the entangled nature of IS_SELECTED_ACCOUNT_SUPERVISED
        // and FRE_POLICY they are both handled in this function as one of these properties
        // will get updated before the other.
        final boolean hasPolicy = model.get(SigninFirstRunProperties.FRE_POLICY) != null;
        final boolean isAccountSupervised =
                model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);

        if (isAccountSupervised) {
            view.getBrowserManagedHeaderView().setVisibility(View.VISIBLE);
            view.getPrivacyDisclaimer().setText(R.string.fre_browser_managed_by_parents);
            view.getPrivacyDisclaimer().setCompoundDrawablesRelativeWithIntrinsicBounds(
                    R.drawable.ic_account_child_20dp, 0, 0, 0);
        } else if (hasPolicy) {
            view.getBrowserManagedHeaderView().setVisibility(View.VISIBLE);
            view.getPrivacyDisclaimer().setText(R.string.fre_browser_managed_by_organization);
            view.getPrivacyDisclaimer().setCompoundDrawablesRelativeWithIntrinsicBounds(
                    R.drawable.ic_business, 0, 0, 0);
        } else {
            view.getBrowserManagedHeaderView().setVisibility(View.GONE);
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
                    SigninUtils.getContinueAsButtonText(view.getContext(), profileData));
        }
        updateVisibility(view, model);
    }

    private static void updateVisibility(SigninFirstRunView view, PropertyModel model) {
        final boolean showInitialLoadProgressSpinner =
                model.get(SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER);
        final boolean isSelectedAccountSupervised =
                model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
        final boolean hasPolicy = model.get(SigninFirstRunProperties.FRE_POLICY) != null;
        if (!showInitialLoadProgressSpinner) {
            view.applyVariationsExperiment(!isSelectedAccountSupervised && !hasPolicy);
        }

        final int selectedAccountVisibility = !showInitialLoadProgressSpinner
                        && model.get(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA) != null
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                ? View.VISIBLE
                : View.GONE;
        view.getSelectedAccountView().setVisibility(selectedAccountVisibility);
        view.getExpandIconView().setVisibility(
                selectedAccountVisibility == View.VISIBLE && isSelectedAccountSupervised
                        ? View.INVISIBLE
                        : View.VISIBLE);
        final int dismissButtonVisibility = !showInitialLoadProgressSpinner
                        && model.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)
                        && !isSelectedAccountSupervised
                ? View.VISIBLE
                : View.GONE;
        view.getDismissButtonView().setVisibility(dismissButtonVisibility);

        final int otherElementsVisibility =
                showInitialLoadProgressSpinner ? View.GONE : View.VISIBLE;
        view.getContinueButtonView().setVisibility(otherElementsVisibility);
        view.getFooterView().setVisibility(otherElementsVisibility);
    }

    private static void updateVisibilityOnButtonClick(
            SigninFirstRunView view, PropertyModel model) {
        final boolean showSigninProgressSpinner =
                model.get(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT)
                || model.get(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER);
        final boolean isSelectedAccountSupervised =
                model.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
        final boolean showSigningInText =
                model.get(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT);

        if (showSigninProgressSpinner) {
            // Transition is only used when the progress spinner is shown.
            TransitionManager.beginDelayedTransition(
                    view, new AutoTransition().setStartDelay(300).setDuration(300));
        }
        final int bottomGroupVisibility = showSigninProgressSpinner ? View.INVISIBLE : View.VISIBLE;
        view.getSelectedAccountView().setVisibility(bottomGroupVisibility);
        if (!isSelectedAccountSupervised) {
            // Only adjust dismiss button visibility if it's not already removed for a child user.
            view.getDismissButtonView().setVisibility(bottomGroupVisibility);
        }
        view.getContinueButtonView().setVisibility(bottomGroupVisibility);
        view.getFooterView().setVisibility(bottomGroupVisibility);

        view.getSigninProgressSpinner().setVisibility(
                showSigninProgressSpinner ? View.VISIBLE : View.GONE);
        view.getSigninProgressText().setVisibility(showSigningInText ? View.VISIBLE : View.GONE);
    }

    private SigninFirstRunViewBinder() {}
}
