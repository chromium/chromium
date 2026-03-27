// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the request access button. This class is responsible for listening to changes in the
 * extensions and updating the button accordingly.
 */
@NullMarked
class ExtensionAccessControlButtonMediator implements Destroyable {
    private final ExtensionsToolbarBridge.Observer mToolbarObserver = new ToolbarObserver();
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final PropertyModel mModel;
    private final Callback<Boolean> mVisibilityObserver;
    private final Callback<@Nullable Tab> mTabSupplierObserver =
            (tab) -> refreshRequestAccessButton();

    public ExtensionAccessControlButtonMediator(
            PropertyModel model,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            Callback<Boolean> visibilityObserver) {
        mCurrentTabSupplier = currentTabSupplier;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mModel = model;
        mVisibilityObserver = visibilityObserver;

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
            mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, false);
            mVisibilityObserver.onResult(false);
            return;
        }

        refreshRequestAccessButtonWithWebContents(webContents);
    }

    private void refreshRequestAccessButtonWithWebContents(WebContents webContents) {
        RequestAccessButtonParams params =
                mExtensionsToolbarBridge.getRequestAccessButtonParams(webContents);
        if (params.getExtensionIds().length > 0) {
            mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, true);
            mModel.set(
                    ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION,
                    params.getTooltipText());

            int count = params.getExtensionIds().length;
            mModel.set(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TEXT, count);
            mVisibilityObserver.onResult(true);
        } else {
            mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, false);
            mVisibilityObserver.onResult(false);
        }
    }

    @Override
    public void destroy() {
        mCurrentTabSupplier.removeObserver(mTabSupplierObserver);
        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
    }

    private class ToolbarObserver implements ExtensionsToolbarBridge.Observer {
        @Override
        public void onActiveWebContentsChanged(WebContents webContents) {
            refreshRequestAccessButtonWithWebContents(webContents);
        }

        @Override
        public void onRequestAccessButtonParamsChanged() {
            refreshRequestAccessButton();
        }
    }
}
