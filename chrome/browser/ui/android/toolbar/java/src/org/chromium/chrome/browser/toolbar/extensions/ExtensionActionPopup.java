// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
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

    /** The activity to use for creating views. */
    private final Activity mActivity;

    /** The ID of the extension action this popup is associated with. */
    private final String mActionId;

    /** The content manager for the popup, bridging to native. */
    private final ExtensionActionPopupContents mContents;

    /** The ThinWebView component that renders the extension's HTML content. */
    private final ThinWebView mThinWebView;

    /** The PopupWindow that is displayed on the screen, anchored to a view. */
    private final AnchoredPopupWindow mPopupWindow;

    /** The window of the popup. */
    private final ActivityWindowAndroid mPopupWindowAndroid;

    /** The content view of the popup. */
    private final ContentView mContentView;

    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabObserver;

    /**
     * Constructs an ExtensionActionPopup.
     *
     * @param activity The {@link Activity} to use for creating views.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param anchorView The {@link View} to which the popup will be anchored.
     * @param actionId The ID of the extension action.
     * @param contents The {@link ExtensionActionPopupContents} instance that manages the
     *     WebContents and native communication for this popup. The new {@link ExtensionActionPopup}
     *     instance takes ownership of the provided {@code contents} and will be responsible for
     *     calling its {@code destroy()} method.
     * @param contextMenuPopulatorFactory The {@link ContextMenuPopulatorFactory} to use.
     * @param selectionDropdownMenuDelegate The {@link SelectionDropdownMenuDelegate} to use.
     * @param tabModelSelector The {@link TabModelSelector} to use.
     */
    public ExtensionActionPopup(
            Activity activity,
            WindowAndroid windowAndroid,
            View anchorView,
            String actionId,
            ExtensionActionPopupContents contents,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            TabModelSelector tabModelSelector) {
        mActivity = activity;
        mActionId = actionId;
        mContents = contents;

        WebContents webContents = contents.getWebContents();

        mContentView = ContentView.createContentView(activity, webContents);

        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        mPopupWindowAndroid =
                new ActivityWindowAndroid(
                        activity,
                        /* listenToActivityState= */ true,
                        NullUtil.assumeNonNull(windowAndroid.getIntentRequestTracker()),
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ true) {
                    @Override
                    public @Nullable ModalDialogManager getModalDialogManager() {
                        return windowAndroid.getModalDialogManager();
                    }
                };

        mThinWebView =
                ThinWebViewFactory.create(
                        activity, new ThinWebViewConstraints(), mPopupWindowAndroid);

        if (contextMenuPopulatorFactory != null) {
            ThinWebViewContextMenuItemDelegate itemDelegate =
                    new ThinWebViewContextMenuItemDelegate(webContents);
            contextMenuPopulatorFactory.setItemDelegate(itemDelegate);
        }

        mTabModelSelector = tabModelSelector;
        mTabObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        if (mPopupWindow.isShowing()) {
                            // Due to inherent differences between platforms on focus handling, we
                            // explicitly observe tab changes and dismiss, unlike on Desktop where
                            // the popup is automatically dismissed as it loses focus due to the tab
                            // change.
                            mPopupWindow.dismiss();
                        }
                    }
                };
        mTabModelSelector.addObserver(mTabObserver);

        mThinWebView.attachWebContents(
                webContents,
                mContentView,
                new ThinWebViewAttachParams.Builder()
                        .setContextMenuPopulatorFactory(contextMenuPopulatorFactory)
                        .setSelectionDropdownMenuDelegate(selectionDropdownMenuDelegate)
                        .build());

        mPopupWindow =
                new AnchoredPopupWindow(
                        activity,
                        activity.getWindow().getDecorView(),
                        new ColorDrawable(Color.WHITE),
                        mThinWebView.getView(),
                        new ViewRectProvider(anchorView));

        mPopupWindow.setHorizontalOverlapAnchor(true);
        mPopupWindow.setOutsideTouchable(true);
        mPopupWindow.setAllowNonTouchableSize(true);

        Resources resources = mActivity.getResources();
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
        mTabModelSelector.removeObserver(mTabObserver);
        mPopupWindow.dismiss();
        mThinWebView.destroy();
        mPopupWindowAndroid.destroy();
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
            if (Build.VERSION.SDK_INT >= 34) {
                // Disable transition animations for the popup window. On Android, {@link
                // onLoaded()} is called first, and then {@link resizeDueToAutoResize()} is called.
                // A transition would result in a sliding animation from the original bounds to the
                // updated bounds.
                // TODO(crbug.com/478100096): Figure out what to do for lower API levels.
                ((WindowManager.LayoutParams) mContentView.getRootView().getLayoutParams())
                        .setCanPlayMoveAnimation(false);
            }

            mPopupWindow.setDesiredContentSize(
                    ViewUtils.dpToPx(mActivity, width), ViewUtils.dpToPx(mActivity, height));
        }

        @Override
        public boolean handleKeyboardEvent(WebContents webContents, KeyEvent event) {
            // We send unhandled keyboard events to the main {@link Activity} so that unconsumed
            // keybindings pass through to the application window.
            return mActivity.dispatchKeyEvent(event);
        }

        @Override
        public void onLoaded() {
            mPopupWindow.show();
            mContentView.requestFocus();
        }

        @Override
        public void onClose() {
            mPopupWindow.dismiss();
        }
    }
}
