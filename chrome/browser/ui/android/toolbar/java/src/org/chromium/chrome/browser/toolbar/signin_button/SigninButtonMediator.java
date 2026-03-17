// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator for a signin button on the NTP toolbar. Listens for sign in state changes and drives
 * corresponding changes to the property model values which back the SigninButton view
 * TODO(crbug.com/475816843): Implement empty methods and add remaining functionality.
 */
@NullMarked
final class SigninButtonMediator
        implements ProfileDataCache.Observer,
                IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                BottomSheetSigninAndHistorySyncCoordinator.Delegate {
    private final Context mContext;
    private final PropertyModel mModel;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;
    private @Nullable Profile mProfile;

    // We observe IdentityManager to receive primary account state change notifications.
    private @Nullable IdentityManager mIdentityManager;

    // ProfileDataCache facilitates retrieving the profile picture.
    private @Nullable ProfileDataCache mProfileDataCache;

    /**
     * @param context The {@link Context} to retrieve resources.
     * @param windowAndroid The {@link WindowAndroid} for the current window.
     * @param model The {@link PropertyModel} for the sign-in button.
     * @param profileSupplier The supplier of the current profile.
     * @param signinAndHistorySyncActivityLauncher The {@link SigninAndHistorySyncActivityLauncher}
     *     to launch sign-in and history sync activity.
     * @param activityResultTracker The {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param deviceLockActivityLauncher The launcher for the device lock challenge.
     * @param bottomSheetController The {@link BottomSheetController} to show the sign-in bottom
     *     sheet.
     * @param modalDialogManager The {@link ModalDialogManager} to manage the dialog.
     * @param snackbarManager The {@link SnackbarManager} to show sign-in/sign-out snackbars.
     */
    public SigninButtonMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager) {
        mContext = context;
        mModel = model;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);
    }

    /**
     * {@link SyncService.SyncStateChangedListener} implementation which updates identity error and
     * profile badge if needed.
     */
    @Override
    public void syncStateChanged() {
        // TODO(crbug.com/475816843): Add implementation for necessary override.
    }

    /**
     * {@link IdentityManager.Observer} implementation which updates the signin button when primary
     * account changes.
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)) {
            case PrimaryAccountChangeEvent.Type.SET:
                updateButtonState();
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                updateButtonState();
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                break;
        }
    }

    /**
     * {@link ProfileDataCache.Observer} implementation which updates the image on the toolbar
     * button once the profile image becomes available.
     */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        String primaryEmail =
                CoreAccountInfo.getEmailFrom(
                        assumeNonNull(mIdentityManager).getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        if (profileData.getAccountEmail().equals(primaryEmail)) {
            updateButtonState();
        }
    }

    void updateButtonVisibility(Boolean showButton) {
        mModel.set(SigninButtonProperties.SHOW_BUTTON, showButton);
    }

    private void updateButtonState() {
        if (mProfile == null || mProfile.isOffTheRecord()) {
            assert !mModel.get(SigninButtonProperties.SHOW_BUTTON);
            return;
        }
        @Nullable String email =
                CoreAccountInfo.getEmailFrom(
                        assumeNonNull(mIdentityManager).getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        DisplayableProfileData profileData =
                email == null
                        ? null
                        : assumeNonNull(mProfileDataCache).getProfileDataOrDefault(email);
        // TODO(https://crbug.com/478828569): Replace UserActionableError.NONE once identity error
        // badge functionality has been added.
        mModel.set(
                SigninButtonProperties.CONTENT_DESCRIPTION,
                SigninUtils.getContentDescriptionForIdentityDisc(
                        mContext, profileData, UserActionableError.NONE));
        setAvatarImage(profileData);
    }

    private void setAvatarImage(@Nullable DisplayableProfileData profileData) {
        if (profileData == null) {
            mModel.set(
                    SigninButtonProperties.BUTTON_AVATAR,
                    AppCompatResources.getDrawable(mContext, R.drawable.account_circle));
            mModel.set(
                    SigninButtonProperties.AVATAR_TINT,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
        } else {
            mModel.set(SigninButtonProperties.BUTTON_AVATAR, profileData.getImage());
            mModel.set(SigninButtonProperties.AVATAR_TINT, null);
        }
        mModel.set(SigninButtonProperties.SHOW_AVATAR, true);
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, or set to null if profile is off-the-record.
     */
    private void setProfile(@Nullable Profile profile) {
        mProfile = profile;
        resetProfileDataCache();
        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
            mIdentityManager = null;
        }
        if (profile == null || profile.isOffTheRecord()) {
            return;
        }
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(mIdentityManager).addObserver(this);
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        mContext, mIdentityManager, R.dimen.toolbar_identity_disc_size);
        mProfileDataCache.addObserver(this);
        updateButtonState();
    }

    private void resetProfileDataCache() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }

    /** Call to tear down dependencies. */
    public void destroy() {
        setProfile(null);
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
    }
}
