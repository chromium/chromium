// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ButtonCompat;

/** A dedicated preference for the account settings signout button. */
@NullMarked
public class SignoutButtonPreference extends Preference implements ContainmentItem {
    private Context mContext;
    private Profile mProfile;
    private FragmentManager mFragmentManager;
    private ModalDialogManager mDialogManager;
    private @Nullable OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;

    public SignoutButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.signout_button_view);
    }

    @Initializer
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
                    if (!assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile))
                            .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                        // Clearing the primary account is happening asynchronously, so it is
                        // possible that a sign-out happened in the meantime.
                        return;
                    }
                    // Snackbar won't be visible in the context of this activity, but there's
                    // special handling for it in MainSettings.
                    assumeNonNull(mSnackbarManagerSupplier);
                    SignOutCoordinator.startSignOutFlow(
                            mContext,
                            mProfile,
                            mFragmentManager,
                            mDialogManager,
                            assertNonNull(mSnackbarManagerSupplier.get()),
                            SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                            /* showConfirmDialog= */ false,
                            () -> {});
                });
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return BackgroundStyle.NONE;
    }
}
