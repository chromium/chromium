// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
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

    private ChromeAndroidTask mTask;
    private ExtensionActionListCoordinator mExtensionActionListCoordinator;
    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Override
    public void initializeWithNative(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            ChromeAndroidTask task,
            NullableObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            ThemeColorProvider themeColorProvider) {
        mTask = task;
        extensionToolbarStub.setLayoutResource(R.layout.extension_toolbar_container);
        LinearLayout container = (LinearLayout) extensionToolbarStub.inflate();
        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context,
                        container.findViewById(R.id.extension_action_list),
                        windowAndroid,
                        task,
                        currentTabSupplier);
        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        context,
                        container.findViewById(R.id.extensions_menu_button),
                        themeColorProvider,
                        task,
                        currentTabSupplier,
                        tabCreator);
    }

    @Override
    public void destroy() {
        mExtensionsMenuCoordinator.destroy();
        mExtensionActionListCoordinator.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Filter out events we are not interested in before calling into JNI.
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0) {
            return false;
        }

        ExtensionActionsBridge bridge = ExtensionActionsBridge.get(mTask);
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
