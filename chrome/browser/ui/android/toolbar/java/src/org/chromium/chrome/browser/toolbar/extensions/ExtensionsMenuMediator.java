// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/**
 * Mediator for the extensions menu. This class is responsible for listening to changes in the
 * extensions and updating the model accordingly.
 */
@NullMarked
class ExtensionsMenuMediator implements Destroyable {
    private final ActionsUpdateDelegate mActionsUpdateDelegate = new ActionsUpdateDelegate();
    private final Context mContext;
    private final OneshotSupplier<ChromeAndroidTask> mTaskSupplier;
    private final ObservableSupplier<@Nullable Profile> mProfileSupplier;
    private final Runnable mOnUpdateFinishedRunnable;
    private final Callback<Boolean> mOnExtensionsAvailableCallback;
    private final ExtensionActionsUpdateHelper mExtensionActionsUpdateHelper;
    private final Callback<@Nullable Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private final View mRootView;

    public ExtensionsMenuMediator(
            Context context,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            ModelList extensionModels,
            Runnable onUpdateFinishedRunnable,
            Callback<Boolean> onExtensionsAvailableCallback,
            View rootView) {
        mTaskSupplier = taskSupplier;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mProfileUpdatedCallback);

        mOnUpdateFinishedRunnable = onUpdateFinishedRunnable;
        mOnExtensionsAvailableCallback = onExtensionsAvailableCallback;
        mContext = context;
        mRootView = rootView;

        mExtensionActionsUpdateHelper =
                new ExtensionActionsUpdateHelper(
                        extensionModels,
                        profileSupplier,
                        currentTabSupplier,
                        mActionsUpdateDelegate);
    }

    private static class RelativeViewRectProvider extends RectProvider {
        private final View mAnchorView;
        private final View mParentView;

        RelativeViewRectProvider(View anchorView, View parentView) {
            mAnchorView = anchorView;
            mParentView = parentView;
        }

        /**
         * For {@link AnchoredPopupWindow} to correctly place nested popup windows, we have to make
         * sure to send coordinates relative to the main window of the application, not positions
         * relative to the parent popup window nor the screen.
         */
        @Override
        public Rect getRect() {
            int[] anchorLocation = new int[2];
            mAnchorView.getLocationOnScreen(anchorLocation);

            int[] parentLocation = new int[2];
            mParentView.getLocationOnScreen(parentLocation);

            int x = anchorLocation[0] - parentLocation[0];
            int y = anchorLocation[1] - parentLocation[1];

            return new Rect(x, y, x + mAnchorView.getWidth(), y + mAnchorView.getHeight());
        }
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        // TODO(crbug.com/422307625): Remove this check once extensions are ready for dogfooding.
        boolean extensionsSupported =
                profile != null ? ExtensionActionsBridge.extensionsEnabled(profile) : false;
        mOnExtensionsAvailableCallback.onResult(extensionsSupported);
    }

    private void onPrimaryClick(ListMenuButton buttonView, String actionId) {
        ChromeAndroidTask task = mTaskSupplier.get();
        if (task == null) {
            return;
        }

        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return;
        }

        ExtensionActionContextMenuBridge bridge =
                new ExtensionActionContextMenuBridge(
                        task, actionId, webContents, ContextMenuSource.MENU_ITEM);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext,
                buttonView,
                bridge,
                new RelativeViewRectProvider(buttonView, mRootView),
                mRootView);
    }

    @Override
    public void destroy() {
        mExtensionActionsUpdateHelper.destroy();
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
    }

    private class ActionsUpdateDelegate
            implements ExtensionActionsUpdateHelper.ActionsUpdateDelegate {
        @Override
        public void onUpdateStarted() {}

        @Override
        public ListItem createActionModel(
                ExtensionActionsBridge extensionActionsBridge, int tabId, String actionId) {
            ExtensionAction action = extensionActionsBridge.getAction(actionId, tabId);
            assert action != null;

            Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
            WebContents webContents = currentTab == null ? null : currentTab.getWebContents();

            Bitmap icon =
                    ExtensionActionIconUtil.getActionIcon(
                            mContext, extensionActionsBridge, actionId, tabId, webContents);
            assert icon != null;
            return new ListItem(
                    0,
                    new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                            .with(ExtensionsMenuItemProperties.TITLE, action.getTitle())
                            .with(ExtensionsMenuItemProperties.ICON, icon)
                            .with(
                                    ExtensionsMenuItemProperties.CLICK_LISTENER,
                                    (view) -> onPrimaryClick((ListMenuButton) view, actionId))
                            .build());
        }

        @Override
        public void onUpdateFinished() {
            mOnUpdateFinishedRunnable.run();
        }
    }
}
