// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.widget.ButtonCompat;

/** A dedicated preference for the account settings signout button. */
public class SignoutButtonPreference extends Preference {
    Profile mProfile;

    public SignoutButtonPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signout_button_view);
    }

    public void initialize(Profile profile) {
        mProfile = profile;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat button = (ButtonCompat) holder.findViewById(R.id.sign_out_button);
        button.setOnClickListener(
                (View v) -> {
                    IdentityServicesProvider.get()
                            .getSigninManager(mProfile)
                            .signOut(
                                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                                    /* signOutCallback= */ null,
                                    /* forceWipeUserData= */ false);
                });
    }
}
