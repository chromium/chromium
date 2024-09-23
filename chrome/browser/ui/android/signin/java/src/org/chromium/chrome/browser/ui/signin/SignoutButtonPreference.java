// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ButtonCompat;

/** A dedicated preference for the account settings signout button. */
public class SignoutButtonPreference extends Preference {
    Context mContext;
    Profile mProfile;
    FragmentManager mFragmentManager;
    ModalDialogManager mDialogManager;
    OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;

    public SignoutButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signout_button_view);
    }

    public void initialize(
            Context context,
            Profile profile,
            FragmentManager fragmentManager,
            ModalDialogManager dialogManager) {
        mContext = context;
        mProfile = profile;
        mFragmentManager = fragmentManager;
        mDialogManager = dialogManager;
    }

    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat button = (ButtonCompat) holder.findViewById(R.id.sign_out_button);
        button.setOnClickListener(
                (View v) -> {
                    assert !mProfile.isChild();
                    if (!IdentityServicesProvider.get()
                            .getIdentityManager(mProfile)
                            .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                        // Clearing the primary account is happening asynchronously, so it is
                        // possible that a sign-out happened in the meantime.
                        return;
                    }
                    // Snackbar won't be visible in the context of this activity, but there's
                    // special handling for it in MainSettings.
                    SignOutCoordinator.startSignOutFlow(
                            mContext,
                            mProfile,
                            mFragmentManager,
                            mDialogManager,
                            mSnackbarManagerSupplier.get(),
                            SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                            /* showConfirmDialog= */ false,
                            () -> {});
                });
    }
}
