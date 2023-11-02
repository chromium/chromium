// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.text.SpannableString;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class SigninFirstRunProperties {
    /**
     * This class regroups the policies supported by FRE.
     * When forced sign-in will be supported, we could use an enum field in this class
     * instead of the boolean property.
     */
    static class FrePolicy { public boolean metricsReportingDisabledByPolicy; }

    static final ReadableObjectPropertyKey<OnClickListener> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");
    static final WritableBooleanPropertyKey IS_SELECTED_ACCOUNT_SUPERVISED =
            new WritableBooleanPropertyKey("is_selected_account_supervised");

    // PropertyKey for the button |Continue as ...|
    static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey for the dismiss button
    static final ReadableObjectPropertyKey<OnClickListener> ON_DISMISS_CLICKED =
            new ReadableObjectPropertyKey<>("on_dismiss_clicked");

    // Is not initialized in #createModel(...) to avoid conflicting view changes with
    // ARE_NATIVE_AND_POLICY_LOADED. Will be set when |Continue as ...| is pressed.
    static final WritableBooleanPropertyKey SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT =
            new WritableBooleanPropertyKey("show_signin_progress_spinner_with_text");

    // Is not initialized in #createModel(...) to avoid conflicting view changes with
    // ARE_NATIVE_AND_POLICY_LOADED. Will be set when dismiss button is pressed.
    static final WritableBooleanPropertyKey SHOW_SIGNIN_PROGRESS_SPINNER =
            new WritableBooleanPropertyKey("show_signin_progress_spinner");

    static final WritableBooleanPropertyKey SHOW_INITIAL_LOAD_PROGRESS_SPINNER =
            new WritableBooleanPropertyKey("show_initial_load_progress_spinner");

    static final WritableObjectPropertyKey<FrePolicy> FRE_POLICY =
            new WritableObjectPropertyKey<>("fre_policy");

    static final WritableBooleanPropertyKey IS_SIGNIN_SUPPORTED =
            new WritableBooleanPropertyKey("is_signin_supported");

    static final WritableObjectPropertyKey<CharSequence> FOOTER_STRING =
            new WritableObjectPropertyKey<>("footer_string");

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            ON_SELECTED_ACCOUNT_CLICKED,
            SELECTED_ACCOUNT_DATA,
            IS_SELECTED_ACCOUNT_SUPERVISED,
            ON_CONTINUE_AS_CLICKED,
            ON_DISMISS_CLICKED,
            SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT,
            SHOW_SIGNIN_PROGRESS_SPINNER,
            SHOW_INITIAL_LOAD_PROGRESS_SPINNER,
            FRE_POLICY,
            IS_SIGNIN_SUPPORTED,
            FOOTER_STRING,
    };

    /**
     * Creates a default model for FRE bottom group.
     */
    static PropertyModel createModel(Runnable onSelectedAccountClicked,
            Runnable onContinueAsClicked, Runnable onDismissClicked, boolean isSigninSupported,
            SpannableString footerString) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, v -> onSelectedAccountClicked.run())
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(IS_SELECTED_ACCOUNT_SUPERVISED, false)
                .with(ON_CONTINUE_AS_CLICKED, v -> onContinueAsClicked.run())
                .with(ON_DISMISS_CLICKED, v -> onDismissClicked.run())
                .with(SHOW_INITIAL_LOAD_PROGRESS_SPINNER, true)
                .with(FRE_POLICY, null)
                .with(IS_SIGNIN_SUPPORTED, isSigninSupported)
                .with(FOOTER_STRING, footerString)
                .build();
    }

    private SigninFirstRunProperties() {}
}
