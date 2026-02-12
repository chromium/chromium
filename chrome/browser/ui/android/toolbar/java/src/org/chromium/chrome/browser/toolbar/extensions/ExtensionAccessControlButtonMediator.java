// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Mediator for the request access button. This class is responsible for listening to changes in the
 * extensions and updating the button accordingly.
 */
@NullMarked
class ExtensionAccessControlButtonMediator implements Destroyable {
    private final ExtensionsToolbarBridge.Observer mToolbarObserver = new ToolbarObserver();
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final View mRequestAccessButton;
    private final Callback<@Nullable Tab> mTabSupplierObserver =
            (tab) -> refreshRequestAccessButton();

    public ExtensionAccessControlButtonMediator(
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            View requestAccessButton) {
        mCurrentTabSupplier = currentTabSupplier;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mRequestAccessButton = requestAccessButton;

        mExtensionsToolbarBridge.addObserver(mToolbarObserver);
        mCurrentTabSupplier.addSyncObserverAndPostIfNonNull(mTabSupplierObserver);
        refreshRequestAccessButton();
    }

    private void refreshRequestAccessButton() {
        Tab currentTab = mCurrentTabSupplier.get();
        // Note: If there are no web contents, the extensions toolbar shouldn't be visible at all.
        // We should handle this at the extensions toolbar level.
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;
        if (webContents == null) {
            mRequestAccessButton.setVisibility(View.GONE);
            return;
        }

        RequestAccessButtonParams params =
                mExtensionsToolbarBridge.getRequestAccessButtonParams(webContents);
        if (params.getExtensionIds().length > 0) {
            mRequestAccessButton.setVisibility(View.VISIBLE);
            mRequestAccessButton.setContentDescription(params.getTooltipText());
        } else {
            mRequestAccessButton.setVisibility(View.GONE);
        }
    }

    @Override
    public void destroy() {
        mCurrentTabSupplier.removeObserver(mTabSupplierObserver);
        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
    }

    private class ToolbarObserver implements ExtensionsToolbarBridge.Observer {
        @Override
        public void onActiveWebContentsChanged() {
            refreshRequestAccessButton();
        }

        @Override
        public void onRequestAccessButtonParamsChanged() {
            refreshRequestAccessButton();
        }
    }
}
