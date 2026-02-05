// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;

/**
 * Coordinator for the request access button. This class is responsible for the button that allows
 * extensions to request access to the current site.
 */
@NullMarked
public class ExtensionAccessControlButtonCoordinator implements Destroyable {
    private final ExtensionAccessControlButtonMediator mMediator;

    /**
     * Constructor.
     *
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param extensionsToolbarBridge The bridge to interact with extensions.
     * @param requestAccessButton The button to request access to the current site.
     */
    public ExtensionAccessControlButtonCoordinator(
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            View requestAccessButton) {
        mMediator =
                new ExtensionAccessControlButtonMediator(
                        currentTabSupplier, extensionsToolbarBridge, requestAccessButton);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
    }
}
