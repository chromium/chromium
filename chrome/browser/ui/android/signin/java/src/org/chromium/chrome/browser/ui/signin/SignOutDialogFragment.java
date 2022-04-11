// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Shows the dialog that explains the user the consequences of signing out of Chrome.
 * Calls the listener callback if the user signs out.
 */
public class SignOutDialogFragment
        extends DialogFragment implements DialogInterface.OnClickListener {
    /** The action for this dialog (see ActionType below). */
    private static final String ACTION_TYPE = "ActionType";

    /**
     * The extra key used to specify the GAIA service that triggered this dialog.
     */
    private static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    /**
     * Receives updates when the user clicks "Sign out".
     */
    public interface SignOutDialogListener {
        /**
         * Called when the user clicks "Sign out".
         *
         * @param forceWipeUserData Whether the user selected to wipe local device data.
         */
        void onSignOutClicked(boolean forceWipeUserData);
    }

    @IntDef({ActionType.REVOKE_SYNC_CONSENT, ActionType.CLEAR_PRIMARY_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {
        int REVOKE_SYNC_CONSENT = 0;
        int CLEAR_PRIMARY_ACCOUNT = 1;
    }

    @Nullable
    private CheckBox mWipeUserData;

    /** The action this dialog corresponds to. */
    private @ActionType int mActionType = ActionType.CLEAR_PRIMARY_ACCOUNT;

    /**
     * The GAIA service that's prompted this dialog.
     */
    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    public static SignOutDialogFragment create(
            @ActionType int actionType, @GAIAServiceType int gaiaServiceType) {
        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.TOGGLE_SIGNOUT, gaiaServiceType);
        SignOutDialogFragment signOutFragment = new SignOutDialogFragment();
        Bundle args = new Bundle();
        args.putInt(ACTION_TYPE, actionType);
        args.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, gaiaServiceType);
        signOutFragment.setArguments(args);
        return signOutFragment;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        assert getArguments() != null;
        mActionType = getArguments().getInt(ACTION_TYPE);
        mGaiaServiceType = getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        String domain = IdentityServicesProvider.get()
                                .getSigninManager(Profile.getLastUsedRegularProfile())
                                .getManagementDomain();
        if (domain != null) {
            return createDialogForManagedAccount(domain);
        }

        return createDialog();
    }

    private Dialog createDialogForManagedAccount(String domain) {
        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setTitle(R.string.signout_managed_account_title)
                .setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .setMessage(getString(R.string.signout_managed_account_message, domain))
                .create();
    }

    // TODO(crbug.com/1199759): Unsupress warning.
    @SuppressWarnings("UseGetLayoutInflater")
    private Dialog createDialog() {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog);

        // If the user is allowed to delete browsing history, offer an option to clear all local
        // data from the device as part of sign out.
        // TODO(crbug.com/1294761): extract logic for whether data wiping is allowed into
        // SigninManager.
        if (UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
            LayoutInflater inflater = LayoutInflater.from(builder.getContext());
            View body = inflater.inflate(R.layout.signout_wipe_storage_dialog, null);
            mWipeUserData = body.findViewById(R.id.remove_local_data);
            ((TextView) body.findViewById(android.R.id.message)).setText(R.string.signout_message);
            builder.setView(body);
        } else {
            builder.setMessage(R.string.signout_message);
        }

        // Vary the title based on the action.  The current detail description text set above is
        // suitable for both cases, so is intentionally not set conditionally.
        switch (mActionType) {
            case ActionType.REVOKE_SYNC_CONSENT:
                builder.setTitle(R.string.turn_off_sync_title);
                break;
            case ActionType.CLEAR_PRIMARY_ACCOUNT:
                builder.setTitle(R.string.signout_title);
                break;
        }

        return builder.setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            SigninMetricsUtils.logProfileAccountManagementMenu(
                    ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT, mGaiaServiceType);
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            if (mWipeUserData != null) {
                RecordHistogram.recordBooleanHistogram(
                        "Signin.UserRequestedWipeDataOnSignout", mWipeUserData.isChecked());
            }
            SignOutDialogListener targetFragment = (SignOutDialogListener) getTargetFragment();
            targetFragment.onSignOutClicked(mWipeUserData != null && mWipeUserData.isChecked());
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.SIGNOUT_CANCEL, mGaiaServiceType);
    }
}
