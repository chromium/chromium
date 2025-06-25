// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewStub;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * A service that provides extension-related features for {@link RootUiCoordinator}. It is loaded
 * conditionally when extensions are available on Desktop Android.
 */
@NullMarked
public interface ExtensionService extends Destroyable {
    /** Initializes the service. */
    public void initialize(ObservableSupplier<Profile> profileSupplier);

    /** Inflate the toolbar actions stub. */
    public void inflateExtensionToolbar(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            ObservableSupplier<Tab> currentTabSupplier,
            ThemeColorProvider themeColorProvider);

    /** Instantiates the implementation of {@link ExtensionService} if it is available. */
    @Nullable
    public static ExtensionService maybeCreate(ObservableSupplier<Profile> profileSupplier) {
        ExtensionService service = ServiceLoaderUtil.maybeCreate(ExtensionService.class);
        if (service == null) {
            return null;
        }
        service.initialize(profileSupplier);
        return service;
    }

    /** Whether extensions are enabled. */
    public boolean areExtensionsEnabled();

    /**
     * Dispatches the key event to trigger the corresponding extension action if any.
     *
     * @return Whether the event has been consumed.
     */
    public boolean dispatchKeyEvent(KeyEvent event);
}
