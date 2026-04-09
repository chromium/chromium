// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.Supplier;

/**
 * Coordinator for the request access button. This class is responsible for the button that allows
 * extensions to request access to the current site.
 */
@NullMarked
public class ExtensionAccessControlButtonCoordinator implements Destroyable {
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ExtensionAccessControlButtonMediator mMediator;

    /**
     * Constructor.
     *
     * @param currentTabSupplier Supplies the current {@link Tab}.
     * @param extensionsToolbarBridge The bridge to interact with extensions.
     * @param requestAccessButton The button to request access to the current site.
     * @param visibilityObserver The observer to be notified of visibility changes.
     */
    public ExtensionAccessControlButtonCoordinator(
            PropertyModel model,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            TextView requestAccessButton,
            Callback<Boolean> visibilityObserver,
            Supplier<Boolean> isWindowCompactSupplier) {

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, requestAccessButton, ExtensionAccessControlButtonViewBinder::bind);

        mMediator =
                new ExtensionAccessControlButtonMediator(
                        requestAccessButton.getContext(),
                        model,
                        currentTabSupplier,
                        extensionsToolbarBridge,
                        visibilityObserver,
                        isWindowCompactSupplier);
    }

    public void requestVisibilityUpdate() {
        mMediator.requestVisibilityUpdate();
    }

    @Override
    public void destroy() {
        mChangeProcessor.destroy();
        mMediator.destroy();
    }
}
