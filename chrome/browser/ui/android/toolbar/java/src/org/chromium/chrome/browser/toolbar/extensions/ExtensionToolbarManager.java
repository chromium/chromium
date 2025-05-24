// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;

/** Provides extension-related buttons for {@link ToolbarManager}. */
@NullMarked
public interface ExtensionToolbarManager extends Destroyable {
    /** Initializes the manager. */
    public void initialize(
            Context context,
            ViewStub extensionToolbarStub,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            ThemeColorProvider themeColorProvider);

    /** Instantiates the implementation of {@link ExtensionToolbarManager} if it is available. */
    @Nullable
    public static ExtensionToolbarManager maybeCreate(
            Context context,
            ViewStub extensionToolbarStub,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            ThemeColorProvider themeColorProvider) {
        ExtensionToolbarManager manager =
                ServiceLoaderUtil.maybeCreate(ExtensionToolbarManager.class);
        if (manager == null) {
            return null;
        }
        manager.initialize(
                context,
                extensionToolbarStub,
                profileSupplier,
                currentTabSupplier,
                themeColorProvider);
        return manager;
    }
}
