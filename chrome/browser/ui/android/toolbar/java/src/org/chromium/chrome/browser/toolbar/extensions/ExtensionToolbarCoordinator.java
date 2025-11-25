// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewStub;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.ui.base.WindowAndroid;

/**
 * The coordinator of the extension-related toolbar UI.
 *
 * <p>This interface is always compiled, while the rest of the extension UI code may not be compiled
 * if extensions are not enabled in the current build configuration. Any Java UI code that needs to
 * interact with the extension toolbar UI must go through this interface, instead of interacting
 * directly with any conditionally-compiled extension UI classes, to avoid ClassNotFoundException
 * when those classes are not compiled.
 *
 * <p>Call {@link #maybeCreate()} to instantiate an implementation of this interface ({@link
 * ExtensionToolbarCoordinatorImpl}) when it is available.
 */
@NullMarked
public interface ExtensionToolbarCoordinator extends Destroyable {
    /** Instantiates the implementation if it is available. */
    @Nullable
    static ExtensionToolbarCoordinator maybeCreate(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            TabCreator tabCreator,
            ThemeColorProvider themeColorProvider) {
        ExtensionToolbarCoordinator coordinator =
                ServiceLoaderUtil.maybeCreate(ExtensionToolbarCoordinator.class);
        if (coordinator == null) {
            return null;
        }
        coordinator.initialize(
                context,
                extensionToolbarStub,
                windowAndroid,
                taskSupplier,
                profileSupplier,
                currentTabSupplier,
                tabCreator,
                themeColorProvider);
        return coordinator;
    }

    /**
     * Initializes the coordinator and inflates the UI.
     *
     * <p>This method must be called exactly once on initialization by {@link #maybeCreate()}. It is
     * illegal to call it multiple times.
     */
    @Initializer
    void initialize(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            TabCreator tabCreator,
            ThemeColorProvider themeColorProvider);

    /**
     * Dispatches the key event to trigger the corresponding extension action if any.
     *
     * @return Whether the event has been consumed.
     */
    boolean dispatchKeyEvent(KeyEvent event);

    /**
     * Updates the ripple background of the extensions menu button
     *
     * <p>This method is typically invoked when the toolbar's tab model changes, such as when
     * transitioning into incognito mode.
     */
    void updateMenuButtonBackground(int backgroundResource);
}
