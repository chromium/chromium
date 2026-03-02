// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
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
                SyncService.SyncStateChangedListener {
    private final Context mContext;
    private final PropertyModel mModel;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;

    public SigninButtonMediator(
            Context context,
            PropertyModel model,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mModel = model;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);
        setButtonState();
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
     * {@link ProfileDataCache.Observer} implementation which updates the image on the toolbar
     * button once the profile image becomes available.
     */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        // TODO(crbug.com/475816843): Add implementation for necessary override.
    }

    void updateButtonVisibility(Boolean showButton) {
        mModel.set(SigninButtonProperties.SHOW_BUTTON, showButton);
    }

    private void setButtonState() {
        Drawable buttonAvatar = AppCompatResources.getDrawable(mContext, R.drawable.account_circle);
        mModel.set(SigninButtonProperties.BUTTON_AVATAR, buttonAvatar);
        mModel.set(SigninButtonProperties.SHOW_AVATAR, true);
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, or set to null if profile is off-the-record.
     */
    private void setProfile(Profile profile) {
        // TODO(crbug.com/475816843): Add implementation for method.
    }

    /** Call to tear down dependencies. */
    public void destroy() {
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
    }
}
