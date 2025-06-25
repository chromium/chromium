// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionListCoordinator;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsMenuButtonCoordinator;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implements extension-related buttons for {@link RootUiCoordinator}.
 *
 * <p>This class is compiled only when extensions are enabled.
 */
@NullMarked
@ServiceImpl(ExtensionService.class)
public class ExtensionServiceImpl implements ExtensionService {
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    @Nullable private ExtensionActionListCoordinator mExtensionActionListCoordinator;
    @Nullable private ExtensionActionsBridge mExtensionActionsBridge;
    @Nullable private ExtensionsMenuButtonCoordinator mExtensionsMenuButtonCoordinator;

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
    public void inflateExtensionToolbar(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            ObservableSupplier<Tab> currentTabSupplier,
            ThemeColorProvider themeColorProvider) {
        LinearLayout container = (LinearLayout) extensionToolbarStub.inflate();
        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context,
                        container.findViewById(R.id.extension_action_list),
                        windowAndroid,
                        mProfileSupplier,
                        currentTabSupplier);

        mExtensionsMenuButtonCoordinator =
                new ExtensionsMenuButtonCoordinator(
                        context,
                        container.findViewById(R.id.extensions_menu_button),
                        container.findViewById(R.id.extensions_divider),
                        themeColorProvider,
                        mProfileSupplier);
    }

    @Override
    public boolean areExtensionsEnabled() {
        if (mExtensionActionsBridge == null) {
            return false;
        }

        return mExtensionActionsBridge.extensionsEnabled();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Filter out events we are not interested in before calling into JNI.
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0) return false;

        if (mExtensionActionsBridge == null) {
            return false;
        }

        return mExtensionActionsBridge.handleKeyDownEvent(event);
    }

    @Override
    public void destroy() {
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);

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
