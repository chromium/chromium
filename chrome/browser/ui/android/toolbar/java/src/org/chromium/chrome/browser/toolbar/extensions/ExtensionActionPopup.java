// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Manages the display of an extension action's popup UI.
 *
 * <p>This class is responsible for creating and managing an {@link AnchoredPopupWindow} that hosts
 * a {@link ThinWebView} rendering the extension's popup HTML. It owns {@link
 * ExtensionActionPopupContents} which handles the native interactions and WebContents for the
 * popup.
 *
 * <p>The popup's size is determined by the popup content, constrained by hard-coded limits, and
 * managed by the nested {@link ContentsFrame}.
 */
@NullMarked
class ExtensionActionPopup implements Destroyable {
    /** The context to use for creating views. */
    private final Context mContext;

    /** The ID of the extension action this popup is associated with. */
    private final String mActionId;

    /** The content manager for the popup, bridging to native. */
    private final ExtensionActionPopupContents mContents;

    /** The ThinWebView component that renders the extension's HTML content. */
    private final ThinWebView mThinWebView;

    /** The PopupWindow that is displayed on the screen, anchored to a view. */
    private final AnchoredPopupWindow mPopupWindow;

    /**
     * Constructs an ExtensionActionPopup.
     *
     * @param context The {@link Context} to use for creating views.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param anchorView The {@link View} to which the popup will be anchored.
     * @param actionId The ID of the extension action.
     * @param contents The {@link ExtensionActionPopupContents} instance that manages the
     *     WebContents and native communication for this popup. The new {@link ExtensionActionPopup}
     *     instance takes ownership of the provided {@code contents} and will be responsible for
     *     calling its {@code destroy()} method.
     */
    public ExtensionActionPopup(
            Context context,
            WindowAndroid windowAndroid,
            View anchorView,
            String actionId,
            ExtensionActionPopupContents contents) {
        mContext = context;
        mActionId = actionId;
        mContents = contents;

        WebContents webContents = contents.getWebContents();

        ContentView contentView = ContentView.createContentView(context, webContents);

        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        mThinWebView =
                ThinWebViewFactory.create(
                        context,
                        new ThinWebViewConstraints(),
                        NullUtil.assumeNonNull(windowAndroid.getIntentRequestTracker()));
        mThinWebView.attachWebContents(webContents, contentView, null);

        View decorView = ((Activity) anchorView.getContext()).getWindow().getDecorView();
        mPopupWindow =
                new AnchoredPopupWindow(
                        context,
                        decorView,
                        new ColorDrawable(Color.WHITE),
                        mThinWebView.getView(),
                        new ViewRectProvider(anchorView));

        mPopupWindow.setHorizontalOverlapAnchor(true);
        mPopupWindow.setOutsideTouchable(true);
        mPopupWindow.setAllowNonTouchableSize(true);

        Resources resources = mContext.getResources();
        mPopupWindow.setElevation(
                resources.getDimensionPixelSize(R.dimen.extension_action_popup_elevation));

        // Set the content size to the minimum initially.
        mPopupWindow.setDesiredContentSize(
                resources.getDimensionPixelSize(R.dimen.extension_action_popup_min_width),
                resources.getDimensionPixelSize(R.dimen.extension_action_popup_min_height));
        mPopupWindow.setFocusable(true);

        contents.setDelegate(new ContentsDelegate());
    }

    /** Cleans up resources used by this popup. */
    @Override
    public void destroy() {
        mPopupWindow.dismiss();
        mThinWebView.destroy();
        mContents.destroy();
    }

    /** Returns the ID of the extension action this popup represents. */
    public String getActionId() {
        return mActionId;
    }

    /** Triggers the loading of the initial page for the extension popup. */
    public void loadInitialPage() {
        mContents.loadInitialPage();
    }

    /** Adds a listener that will be notified when the popup window is dismissed. */
    public void addOnDismissListener(OnDismissListener listener) {
        mPopupWindow.addOnDismissListener(listener);
    }

    private class ContentsDelegate implements ExtensionActionPopupContents.Delegate {
        @Override
        public void resizeDueToAutoResize(int width, int height) {
            mPopupWindow.setDesiredContentSize(
                    ViewUtils.dpToPx(mContext, width), ViewUtils.dpToPx(mContext, height));
        }

        @Override
        public void onLoaded() {
            mPopupWindow.show();
        }

        @Override
        public void onClose() {
            mPopupWindow.dismiss();
        }
    }
}
