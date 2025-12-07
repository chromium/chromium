// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class ExtensionActionListMediator implements Destroyable {
    private static final String TAG = "EALMediator";

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModels;
    private final OneshotSupplier<ChromeAndroidTask> mTaskSupplier;
    private final ExtensionActionsUpdateHelper mExtensionActionsUpdateHelper;

    private final ActionsUpdateDelegate mActionsUpdateDelegate = new ActionsUpdateDelegate();

    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    @Nullable private ExtensionActionPopup mCurrentPopup;

    public ExtensionActionListMediator(
            Context context,
            WindowAndroid windowAndroid,
            ModelList models,
            OneshotSupplier<ChromeAndroidTask> taskSupplier,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModels = models;
        mTaskSupplier = taskSupplier;

        mExtensionActionsUpdateHelper =
                new ExtensionActionsUpdateHelper(
                        mModels, profileSupplier, currentTabSupplier, mActionsUpdateDelegate);
    }

    @Override
    public void destroy() {
        closePopup();
        assert mCurrentPopup == null;
        mExtensionActionsUpdateHelper.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    private void onPrimaryClick(View buttonView, String actionId) {
        ExtensionActionsBridge extensionActionsBridge =
                mExtensionActionsUpdateHelper.getExtensionActionsBridge();
        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        if (extensionActionsBridge == null || currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            // TODO(crbug.com/385985177): Revisit how to handle this case.
            return;
        }

        @ShowAction
        int showAction =
                extensionActionsBridge.runAction(actionId, currentTab.getId(), webContents);
        switch (showAction) {
            case ShowAction.NONE:
                break;
            case ShowAction.SHOW_POPUP:
                openPopup(buttonView, actionId);
                break;
            case ShowAction.TOGGLE_SIDE_PANEL:
                Log.e(TAG, "Extension side panels are not implemented yet");
                break;
        }
    }

    private void openPopup(View buttonView, String actionId) {
        // TODO(crbug.com/385987224): Do not open a popup again when the user clicks the action
        // button while its popup is open.
        closePopup();

        ChromeAndroidTask task = mTaskSupplier.get();
        if (task == null) {
            return;
        }

        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        if (currentTab == null) {
            return;
        }
        int tabId = currentTab.getId();

        ExtensionActionPopupContents contents =
                ExtensionActionPopupContents.create(task, actionId, tabId);
        assert mCurrentPopup == null;
        mCurrentPopup =
                new ExtensionActionPopup(mContext, mWindowAndroid, buttonView, actionId, contents);
        mCurrentPopup.loadInitialPage();
        mCurrentPopup.addOnDismissListener(this::closePopup);
    }

    private void onContextClick(ListMenuButton buttonView, String actionId) {
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
                        task, actionId, webContents, ContextMenuSource.TOOLBAR_ACTION);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext, buttonView, bridge, MenuBuilderHelper.getRectProvider(buttonView), null);
    }

    private void closePopup() {
        if (mCurrentPopup == null) {
            return;
        }
        assert mExtensionActionsUpdateHelper.getExtensionActionsBridge() != null;

        // Clear mCurrentPopup now to avoid calling closePopup recursively via OnDismissListener.
        ExtensionActionPopup popup = mCurrentPopup;
        mCurrentPopup = null;
        popup.destroy();
    }

    private class ActionsUpdateDelegate
            implements ExtensionActionsUpdateHelper.ActionsUpdateDelegate {
        @Override
        public void onUpdateStarted() {
            closePopup();
            assert mCurrentPopup == null;
        }

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
                    ListItemType.EXTENSION_ACTION,
                    new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                            .with(ExtensionActionButtonProperties.ICON, icon)
                            .with(ExtensionActionButtonProperties.ID, action.getId())
                            .with(
                                    ExtensionActionButtonProperties.ON_CLICK_LISTENER,
                                    (view) -> onPrimaryClick(view, actionId))
                            .with(
                                    ExtensionActionButtonProperties.ON_CONTEXT_CLICK_LISTENER,
                                    (view) -> {
                                        onContextClick((ListMenuButton) view, actionId);
                                        return false;
                                    })
                            .with(ExtensionActionButtonProperties.TITLE, action.getTitle())
                            .build());
        }

        @Override
        public void onUpdateFinished() {}
    }
}
