// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the request access button. This class is responsible for listening to changes in the
 * extensions and updating the button accordingly.
 */
@NullMarked
class ExtensionAccessControlButtonMediator implements Destroyable {
    private static final long CONFIRMATION_DISPLAY_DURATION = 4000L;
    private final ExtensionsToolbarBridge.Observer mToolbarObserver = new ToolbarObserver();
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final Context mContext;
    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final PropertyModel mModel;
    private final Callback<Boolean> mVisibilityObserver;
    private boolean mIsShowingAllowedText;
    private @Nullable WebContents mWebContentsShowingAllowedText;
    private final Callback<@Nullable Tab> mTabSupplierObserver =
            (tab) -> refreshRequestAccessButton();
    private final Runnable mClearAllowedTextRunnable =
            () -> {
                mIsShowingAllowedText = false;
                mWebContentsShowingAllowedText = null;
                refreshRequestAccessButton();
            };

    public ExtensionAccessControlButtonMediator(
            Context context,
            PropertyModel model,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            Callback<Boolean> visibilityObserver) {
        mContext = context;
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

        clearAllowedTextStateIfDifferentWebContents(webContents);
        if (mIsShowingAllowedText) return;
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
            mModel.set(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT, count);
            mVisibilityObserver.onResult(true);
            mModel.set(
                    ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CLICK_LISTENER,
                    (v) -> {
                        mIsShowingAllowedText = true;
                        mWebContentsShowingAllowedText = webContents;
                        mModel.set(
                                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT,
                                -1); // use -1 as a special value for Allowed
                        mModel.set(
                                ExtensionsToolbarProperties
                                        .REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION,
                                mContext.getString(
                                        R.string.extensions_request_access_button_dismissed_text));
                        mModel.set(
                                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CLICK_LISTENER,
                                null); // Disable further clicks

                        mExtensionsToolbarBridge.onRequestAccessButtonClicked(webContents);

                        // We post a delayed task to re-evaluate the button state so it naturally
                        // disappears after the user has a moment to see the "Allowed" text.
                        // We store the Runnable and cancel it in destroy() to prevent crashes
                        // if the mediator is destroyed before the timeout elapses.
                        ThreadUtils.getUiThreadHandler().removeCallbacks(mClearAllowedTextRunnable);
                        ThreadUtils.postOnUiThreadDelayed(
                                mClearAllowedTextRunnable, CONFIRMATION_DISPLAY_DURATION);
                    });
        } else {
            mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, false);
            mVisibilityObserver.onResult(false);
        }
    }

    /**
     * Clears the "Allowed" text state if the given WebContents is different from the one currently
     * showing it.
     */
    private void clearAllowedTextStateIfDifferentWebContents(@Nullable WebContents webContents) {
        if (mIsShowingAllowedText
                && (webContents == null || !webContents.equals(mWebContentsShowingAllowedText))) {
            ThreadUtils.getUiThreadHandler().removeCallbacks(mClearAllowedTextRunnable);
            mIsShowingAllowedText = false;
            mWebContentsShowingAllowedText = null;
        }
    }

    @Override
    public void destroy() {
        ThreadUtils.getUiThreadHandler().removeCallbacks(mClearAllowedTextRunnable);
        mCurrentTabSupplier.removeObserver(mTabSupplierObserver);
        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
    }

    private class ToolbarObserver implements ExtensionsToolbarBridge.Observer {
        @Override
        public void onActiveWebContentsChanged(WebContents webContents) {
            clearAllowedTextStateIfDifferentWebContents(webContents);
            if (mIsShowingAllowedText) return;
            refreshRequestAccessButtonWithWebContents(webContents);
        }

        @Override
        public void onRequestAccessButtonParamsChanged() {
            refreshRequestAccessButton();
        }
    }
}
