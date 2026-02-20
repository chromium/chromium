// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.ui.listmenu.ListMenuButton;

/**
 * Coordinator for the extensions menu and the request access button. This class is responsible for
 * the buttons and the menu.
 */
@NullMarked
public class ExtensionsMenuAndAccessControlButtonCoordinator implements Destroyable {
    private final ExtensionsMenuCoordinator mExtensionsMenuCoordinator;
    private final ExtensionAccessControlButtonCoordinator mExtensionAccessControlButtonCoordinator;

    /**
     * Constructor.
     *
     * @param context The context for this component.
     * @param extensionsMenuButton The puzzle icon in the toolbar.
     * @param themeColorProvider The provider for theme colors.
     * @param task Supplies the {@link ChromeAndroidTask}.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     * @param extensionsToolbarBridge The bridge to interact with extensions.
     * @param requestAccessButton The button to request access to the current site.
     */
    public ExtensionsMenuAndAccessControlButtonCoordinator(
            Context context,
            ListMenuButton extensionsMenuButton,
            ThemeColorProvider themeColorProvider,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            View requestAccessButton) {
        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        context,
                        extensionsMenuButton,
                        themeColorProvider,
                        task,
                        profile,
                        currentTabSupplier,
                        tabCreator,
                        extensionsToolbarBridge);

        mExtensionAccessControlButtonCoordinator =
                new ExtensionAccessControlButtonCoordinator(
                        currentTabSupplier, extensionsToolbarBridge, requestAccessButton);
    }

    public void updateButtonBackground(int backgroundResource) {
        mExtensionsMenuCoordinator.updateButtonBackground(backgroundResource);
    }

    @Override
    public void destroy() {
        mExtensionAccessControlButtonCoordinator.destroy();
        mExtensionsMenuCoordinator.destroy();
    }
}
