// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implements extension-related buttons for {@link ToolbarManager}.
 *
 * <p>This class is compiled only when extensions are enabled.
 */
@NullMarked
@ServiceImpl(ExtensionToolbarManager.class)
public class ExtensionToolbarManagerImpl implements ExtensionToolbarManager {
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    @Nullable private ExtensionActionListCoordinator mExtensionActionListCoordinator;

    public ExtensionToolbarManagerImpl() {}

    @Override
    public void initialize(
            Context context,
            ViewStub extensionToolbarStub,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier) {
        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context, extensionToolbarStub, profileSupplier, currentTabSupplier);
    }

    @Override
    public void destroy() {
        if (mExtensionActionListCoordinator != null) {
            mExtensionActionListCoordinator.destroy();
            mExtensionActionListCoordinator = null;
        }
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
