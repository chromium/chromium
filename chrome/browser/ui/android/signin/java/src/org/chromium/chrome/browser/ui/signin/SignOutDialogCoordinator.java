// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A confirmation dialog for signing out and/or wiping device data. The checkbox to wipe data is not
 * shown for managed accounts. This dialog can be used to only turn off sync without signing out
 * child accounts that are syncing.
 */
public class SignOutDialogCoordinator {
    /**
     * Receives updates when the user interacts with the dialog buttons.
     */
    public interface Listener {
        /**
         * Notifies when the positive button in this dialog was pressed.
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

    private final CheckBox mCheckBox;
    private final @GAIAServiceType int mGaiaServiceType;
    private final Listener mListener;
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;

    /**
     * Shows a dialog asking users to confirm whether they want to sign out. Optionally in case of
     * non-managed users and users who are able to delete browser history, offer an option to clear
     * all local data from the device as part of sign out. Child accounts may not be able to sign
     * out (as Child accounts are signed in by force) and may only turn off sync with the option to
     * clear all local data provided they are syncing.
     * @param context          Context to create the view.
     * @param dialogManager    A ModalDialogManager that manages the dialog.
     * @param listener         Callback to be called when the user taps on the positive button.
     * @param actionType       The action this dialog corresponds to.
     * @param gaiaServiceType  The GAIA service that's prompted this dialog.
     */
    @MainThread
    public static void show(Context context, ModalDialogManager dialogManager, Listener listener,
            @ActionType int actionType, @GAIAServiceType int gaiaServiceType) {
        new SignOutDialogCoordinator(context, dialogManager, listener, actionType, gaiaServiceType);
    }

    private static View inflateView(
            Context context, String managedDomain, @ActionType int actionType) {
        final View view =
                LayoutInflater.from(context).inflate(R.layout.signout_wipe_storage_dialog, null);
        ((TextView) view.findViewById(android.R.id.message))
                .setText(getMessage(context, managedDomain));

        return view;
    }

    private static @StringRes int getTitleRes(String managedDomain, @ActionType int actionType) {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return R.string.signout_title;
        }
        if (managedDomain != null) {
            return R.string.signout_managed_account_title;
        }
        switch (actionType) {
            case ActionType.REVOKE_SYNC_CONSENT:
                return R.string.turn_off_sync_title;
            case ActionType.CLEAR_PRIMARY_ACCOUNT:
                return R.string.turn_off_sync_and_signout_title;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for actionType: " + actionType);
        }
    }

    private static String getMessage(Context context, String managedDomain) {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return context.getString(R.string.signout_message);
        }
        if (managedDomain != null) {
            return context.getString(R.string.signout_managed_account_message, managedDomain);
        }
        return context.getString(R.string.turn_off_sync_and_signout_message);
    }

    private static int getCheckBoxVisibility(String managedDomain) {
        // TODO(crbug.com/1294761): extract logic for whether data wiping is allowed into
        // SigninManager.
        final boolean allowDeletingData = UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                  .getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY);
        final boolean hasSyncConsent =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC);
        final boolean showCheckBox = (managedDomain == null) && allowDeletingData && hasSyncConsent;
        return showCheckBox ? View.VISIBLE : View.GONE;
    }

    @VisibleForTesting
    @MainThread
    SignOutDialogCoordinator(Context context, ModalDialogManager dialogManager, Listener listener,
            @ActionType int actionType, @GAIAServiceType int gaiaServiceType) {
        final String managedDomain = IdentityServicesProvider.get()
                                             .getSigninManager(Profile.getLastUsedRegularProfile())
                                             .getManagementDomain();
        final View view = inflateView(context, managedDomain, actionType);
        mCheckBox = view.findViewById(R.id.remove_local_data);
        mCheckBox.setVisibility(getCheckBoxVisibility(managedDomain));

        mGaiaServiceType = gaiaServiceType;
        mListener = listener;
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.TITLE,
                                 context.getString(getTitleRes(managedDomain, actionType)))
                         .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                 context.getString(R.string.continue_button))
                         .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                 context.getString(R.string.cancel))
                         .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                         .with(ModalDialogProperties.CUSTOM_VIEW, view)
                         .with(ModalDialogProperties.CONTROLLER, createController())
                         .build();
        mDialogManager = dialogManager;

        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.TOGGLE_SIGNOUT, gaiaServiceType);
        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    mListener.onSignOutClicked(
                            mCheckBox.getVisibility() == View.VISIBLE && mCheckBox.isChecked());
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ButtonType.NEGATIVE) {
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                    SigninMetricsUtils.logProfileAccountManagementMenu(
                            ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT, mGaiaServiceType);
                } else {
                    SigninMetricsUtils.logProfileAccountManagementMenu(
                            ProfileAccountManagementMetrics.SIGNOUT_CANCEL, mGaiaServiceType);
                }
            }
        };
    }

    @VisibleForTesting
    @MainThread
    View getDialogViewForTesting() {
        return mModel.get(ModalDialogProperties.CUSTOM_VIEW);
    }

    @VisibleForTesting
    @MainThread
    void dismissDialogForTesting() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTIVITY_DESTROYED);
    }
}
