// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;

/** The implementation of {@link ExtensionService}. */
@NullMarked
@ServiceImpl(ExtensionService.class)
public class ExtensionServiceImpl implements ExtensionService {
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    @Nullable private ExtensionActionsBridge mExtensionActionsBridge;

    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private ObservableSupplier<Profile> mProfileSupplier;
    @Nullable private Profile mProfile;

    public ExtensionServiceImpl() {}

    @Override
    @Initializer
    public void initialize(ObservableSupplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;

        mProfileSupplier.addObserver(mProfileUpdatedCallback);
    }

    @Override
    public boolean areExtensionsEnabled() {
        if (mExtensionActionsBridge == null) {
            return false;
        }

        return mExtensionActionsBridge.extensionsEnabled();
    }

    @Override
    public void destroy() {
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mExtensionActionsBridge = null;
        mProfile = profile;

        if (mProfile != null) {
            mExtensionActionsBridge = ExtensionActionsBridge.get(mProfile);
        }
    }
}
