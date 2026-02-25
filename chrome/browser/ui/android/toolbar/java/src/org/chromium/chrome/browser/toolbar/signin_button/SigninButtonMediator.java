// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.signin_button;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

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
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;

    public SigninButtonMediator(MonotonicObservableSupplier<Profile> profileSupplier) {
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
     * {@link ProfileDataCache.Observer} implementation which updates the image on the toolbar
     * button once the profile image becomes available.
     */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        // TODO(crbug.com/475816843): Add implementation for necessary override.
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
