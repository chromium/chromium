// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.base.WindowAndroid;

/** The implementation of {@link ExtensionToolbarCoordinator}. */
@NullMarked
@ServiceImpl(ExtensionToolbarCoordinator.class)
public class ExtensionToolbarCoordinatorImpl implements ExtensionToolbarCoordinator {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private final Callback<@Nullable Profile> mProfileUpdatedCallback =
            (profile) -> mCurrentProfile = profile;

    private ObservableSupplier<@Nullable Profile> mProfileSupplier;
    private ExtensionActionListCoordinator mExtensionActionListCoordinator;
    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    private @Nullable Profile mCurrentProfile;

    @Override
    public void initialize(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            TabCreator tabCreator,
            ThemeColorProvider themeColorProvider) {
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mProfileUpdatedCallback);

        extensionToolbarStub.setLayoutResource(R.layout.extension_toolbar_container);
        LinearLayout container = (LinearLayout) extensionToolbarStub.inflate();
        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context,
                        container.findViewById(R.id.extension_action_list),
                        windowAndroid,
                        taskSupplier,
                        profileSupplier,
                        currentTabSupplier);
        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        context,
                        container.findViewById(R.id.extensions_menu_button),
                        container.findViewById(R.id.extensions_divider),
                        themeColorProvider,
                        taskSupplier,
                        profileSupplier,
                        currentTabSupplier,
                        tabCreator);
    }

    @Override
    public void destroy() {
        mExtensionsMenuCoordinator.destroy();
        mExtensionActionListCoordinator.destroy();
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Filter out events we are not interested in before calling into JNI.
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0) {
            return false;
        }

        if (mCurrentProfile == null) {
            return false;
        }

        ExtensionActionsBridge bridge = ExtensionActionsBridge.get(mCurrentProfile);
        if (bridge == null) {
            return false;
        }

        ExtensionActionsBridge.HandleKeyEventResult result = bridge.handleKeyDownEvent(event);
        if (result.handled) {
            return true;
        }
        if (result.actionId.isEmpty()) {
            return false;
        }

        mExtensionActionListCoordinator.click(result.actionId);
        return true;
    }

    @Override
    public void updateMenuButtonBackground(int backgroundResource) {
        mExtensionsMenuCoordinator.updateButtonBackground(backgroundResource);
    }
}
