// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.function.Supplier;

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
    private final Supplier<Boolean> mIsWindowCompactSupplier;
    private @Nullable PropertyModel mMessage;
    private String[] mMessageExtensionIds = new String[0];
    private @Nullable WebContents mMessageWebContents;
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
            Callback<Boolean> visibilityObserver,
            Supplier<Boolean> isWindowCompactSupplier) {
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mModel = model;
        mVisibilityObserver = visibilityObserver;
        mIsWindowCompactSupplier = isWindowCompactSupplier;

        mExtensionsToolbarBridge.addObserver(mToolbarObserver);
        mCurrentTabSupplier.addSyncObserverAndPostIfNonNull(mTabSupplierObserver);
        refreshRequestAccessButton();
    }

    public void requestVisibilityUpdate() {
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
            int count = params.getExtensionIds().length;
            mModel.set(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT, count);

            if (mIsWindowCompactSupplier.get()) {
                mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, false);
                mVisibilityObserver.onResult(false);
                showRequestAccessMessage(webContents, params);
            } else {
                dismissMessage(webContents);
                showRequestAccessButton(webContents, params);
            }
        } else {
            dismissMessage(webContents);
            mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, false);
            mVisibilityObserver.onResult(false);
        }
    }

    private void showRequestAccessMessage(
            WebContents webContents, RequestAccessButtonParams params) {
        MessageDispatcher dispatcher =
                MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
        if (dispatcher == null) return;

        if (mMessage != null) {
            if (mMessageWebContents == webContents
                    && Arrays.equals(mMessageExtensionIds, params.getExtensionIds())) {
                return;
            }
            dismissMessage(mMessageWebContents != null ? mMessageWebContents : webContents);
        }

        mMessageExtensionIds = params.getExtensionIds();
        mMessageWebContents = webContents;
        mMessage = createMessageModel(webContents, params);
        dispatcher.enqueueWindowScopedMessage(mMessage, false);
    }

    private PropertyModel createMessageModel(
            WebContents webContents, RequestAccessButtonParams params) {
        int count = params.getExtensionIds().length;
        String title;
        Drawable iconDrawable = null;
        int iconResourceId = R.drawable.chrome_extension;
        String buttonText =
                mContext.getString(
                        R.string.extensions_menu_requests_access_section_allow_button_text);

        if (count == 1) {
            String extensionId = params.getExtensionIds()[0];
            ExtensionAction action = mExtensionsToolbarBridge.getAction(extensionId, webContents);
            title =
                    mContext.getString(
                            R.string.extensions_request_access_message_title_single_extension,
                            action != null ? action.getName() : "");

            Bitmap iconBitmap =
                    ExtensionActionIconUtil.getIcon(
                            mContext,
                            webContents.getTopLevelNativeWindow(),
                            mExtensionsToolbarBridge,
                            extensionId,
                            webContents);
            if (iconBitmap != null) {
                iconDrawable = new BitmapDrawable(mContext.getResources(), iconBitmap);
            }
        } else {
            String template = mContext.getString(R.string.extensions_request_access_button);
            title = template.replace("$1", String.valueOf(count));
        }

        PropertyModel.Builder builder =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.EXTENSIONS_REQUEST_ACCESS)
                        .with(MessageBannerProperties.TITLE, title)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, buttonText)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    handleAccessGranted(webContents);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    mMessage = null;
                                    mMessageExtensionIds = new String[0];
                                    mMessageWebContents = null;
                                });

        if (count == 1) {
            builder.with(
                    MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE);
        }

        if (iconDrawable != null) {
            builder.with(MessageBannerProperties.ICON, iconDrawable);
        } else {
            builder.with(MessageBannerProperties.ICON_RESOURCE_ID, iconResourceId);
        }

        return builder.build();
    }

    private void showRequestAccessButton(
            WebContents webContents, RequestAccessButtonParams params) {
        mModel.set(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE, true);
        mModel.set(
                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION,
                params.getTooltipText());

        mVisibilityObserver.onResult(true);
        mModel.set(
                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CLICK_LISTENER,
                (v) -> {
                    handleAccessGranted(webContents);
                });
    }

    private void handleAccessGranted(WebContents webContents) {
        mIsShowingAllowedText = true;
        mWebContentsShowingAllowedText = webContents;
        mModel.set(
                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT,
                -1); // use -1 as a special value for Allowed
        mModel.set(
                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION,
                mContext.getString(R.string.extensions_request_access_button_dismissed_text));
        mModel.set(
                ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CLICK_LISTENER,
                null); // Disable further clicks

        mExtensionsToolbarBridge.onRequestAccessButtonClicked(webContents);

        // We post a delayed task to re-evaluate the button state so it naturally disappears
        // after the user has a moment to see the "Allowed" text.
        // We store the Runnable and cancel it in destroy() to prevent crashes if the mediator
        // is destroyed before the timeout elapses.
        ThreadUtils.getUiThreadHandler().removeCallbacks(mClearAllowedTextRunnable);
        ThreadUtils.postOnUiThreadDelayed(mClearAllowedTextRunnable, CONFIRMATION_DISPLAY_DURATION);
    }

    private void dismissMessage(@Nullable WebContents webContents) {
        if (mMessage == null) return;

        if (webContents != null && !webContents.isDestroyed()) {
            MessageDispatcher dispatcher =
                    MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
            if (dispatcher != null) {
                dispatcher.dismissMessage(mMessage, DismissReason.UNKNOWN);
            }
        }
        mMessage = null;
        mMessageExtensionIds = new String[0];
        mMessageWebContents = null;
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

        if (mMessageWebContents != null) {
            dismissMessage(mMessageWebContents);
        } else {
            Tab currentTab = mCurrentTabSupplier.get();
            if (currentTab != null && currentTab.getWebContents() != null) {
                dismissMessage(currentTab.getWebContents());
            }
        }
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
