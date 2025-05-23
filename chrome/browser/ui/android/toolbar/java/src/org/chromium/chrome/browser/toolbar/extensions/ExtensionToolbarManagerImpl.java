// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.listmenu.ListMenuButton;

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
    @Nullable private ExtensionsMenuButtonCoordinator mExtensionsMenuButtonCoordinator;

    public ExtensionToolbarManagerImpl() {}

    @Override
    public void initialize(
            Context context,
            ViewStub extensionToolbarStub,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            ThemeColorProvider themeColorProvider) {
        LinearLayout container = (LinearLayout) extensionToolbarStub.inflate();

        LinearLayout actionListContainer = container.findViewById(R.id.extension_action_list);
        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context, actionListContainer, profileSupplier, currentTabSupplier);

        ListMenuButton extensionsMenuButton = container.findViewById(R.id.extensions_menu_button);
        mExtensionsMenuButtonCoordinator =
                new ExtensionsMenuButtonCoordinator(
                        context, extensionsMenuButton, themeColorProvider);
    }

    @Override
    public void destroy() {
        if (mExtensionActionListCoordinator != null) {
            mExtensionActionListCoordinator.destroy();
            mExtensionActionListCoordinator = null;
        }
        if (mExtensionsMenuButtonCoordinator != null) {
            mExtensionsMenuButtonCoordinator.destroy();
            mExtensionsMenuButtonCoordinator = null;
        }
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
