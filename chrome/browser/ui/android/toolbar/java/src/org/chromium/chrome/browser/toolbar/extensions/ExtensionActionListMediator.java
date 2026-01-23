// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class ExtensionActionListMediator implements Destroyable {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModels;
    private final ChromeAndroidTask mTask;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionActionListContainer mContainer;

    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final ToolbarDelegate mToolbarDelegate = new ToolbarDelegate();
    private final ToolbarObserver mToolbarObserver = new ToolbarObserver();

    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    @Nullable private ExtensionActionPopup mCurrentPopup;
    @Nullable private String mCurrentPopupActionId;

    public ExtensionActionListMediator(
            Context context,
            WindowAndroid windowAndroid,
            ModelList models,
            ChromeAndroidTask task,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionActionListContainer container,
            ExtensionsToolbarBridge extensionsToolbarBridge) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModels = models;
        mTask = task;
        mCurrentTabSupplier = currentTabSupplier;
        mContainer = container;
        mExtensionsToolbarBridge = extensionsToolbarBridge;

        mExtensionsToolbarBridge.setDelegate(mToolbarDelegate);
        mExtensionsToolbarBridge.addObserver(mToolbarObserver);
        reconcileActionItems();
    }

    @Override
    public void destroy() {
        closePopup();
        assert mCurrentPopup == null;
        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
        mExtensionsToolbarBridge.setDelegate(null);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    // Reconciles the current list of models with the list of IDs from the
    // bridge. This handles additions, removals, and reordering without
    // rebuilding the whole list.
    @VisibleForTesting
    void reconcileActionItems() {
        String[] actionIds = mExtensionsToolbarBridge.getPinnedActionIds();

        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        // O(N^2).
        int currentModelIndex = 0;
        for (String actionId : actionIds) {
            ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId);
            if (action == null) {
                continue;
            }

            if (currentModelIndex < mModels.size()
                    && getActionIdForIndex(currentModelIndex).equals(actionId)) {
                currentModelIndex++;
                continue;
            }

            int indexInModels = findIndexForId(actionId, currentModelIndex + 1);

            if (indexInModels != -1) {
                mModels.move(indexInModels, currentModelIndex);
            } else {
                mModels.add(currentModelIndex, createListItem(action, webContents));
            }

            currentModelIndex++;
        }

        while (mModels.size() > currentModelIndex) {
            if (getActionIdForIndex(currentModelIndex).equals(mCurrentPopupActionId)) {
                closePopup();
            }
            mModels.removeAt(currentModelIndex);
        }
    }

    private ListItem createListItem(ExtensionAction action, @Nullable WebContents webContents) {
        String actionId = action.getId();

        Bitmap icon = getIconForAction(actionId, webContents);

        return new ListItem(
                ListItemType.EXTENSION_ACTION,
                new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                        .with(ExtensionActionButtonProperties.ICON, icon)
                        .with(ExtensionActionButtonProperties.ID, actionId)
                        .with(
                                ExtensionActionButtonProperties.ON_CLICK_LISTENER,
                                (view) -> onPrimaryClick(actionId))
                        .with(
                                ExtensionActionButtonProperties.ON_CONTEXT_CLICK_LISTENER,
                                (view) -> {
                                    onContextClick((ListMenuButton) view, actionId);
                                    return false;
                                })
                        .with(ExtensionActionButtonProperties.TITLE, action.getTitle())
                        .build());
    }

    @VisibleForTesting
    Bitmap getIconForAction(String actionId, @Nullable WebContents webContents) {
        Bitmap icon =
                ExtensionActionIconUtil.getIcon(
                        mContext, mExtensionsToolbarBridge, actionId, webContents);
        assert icon != null;
        return icon;
    }

    @VisibleForTesting
    void removeActionItem(String actionId) {
        if (mCurrentPopupActionId != null && mCurrentPopupActionId.equals(actionId)) {
            closePopup();
        }

        int index = findIndexForId(actionId, 0);
        if (index != -1) {
            mModels.removeAt(index);
        }
    }

    // Updates model properties while keeping it in place.
    @VisibleForTesting
    void updateActionProperties(String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        int index = findIndexForId(actionId, 0);
        if (index == -1) {
            return;
        }

        ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId);
        if (action == null) {
            return;
        }

        Bitmap icon = getIconForAction(actionId, webContents);
        mModels.get(index).model.set(ExtensionActionButtonProperties.ICON, icon);

        mModels.get(index).model.set(ExtensionActionButtonProperties.TITLE, action.getTitle());
    }

    // Finds the model for {@code actionId} inside {@code mModels}, and returns
    // the index if it exists. If not, returns -1.
    private int findIndexForId(String actionId, int startIndex) {
        for (int i = startIndex; i < mModels.size(); i++) {
            if (getActionIdForIndex(i).equals(actionId)) {
                return i;
            }
        }
        return -1;
    }

    // Returns the {@code actionId} for the {@code index}th model inside {@code mModels}.
    private String getActionIdForIndex(int index) {
        assert index < mModels.size();
        return mModels.get(index).model.get(ExtensionActionButtonProperties.ID);
    }

    private void onPrimaryClick(String actionId) {
        mExtensionsToolbarBridge.executeUserAction(actionId, InvocationSource.TOOLBAR_BUTTON);
    }

    private void triggerPopup(String actionId, long nativeHostPtr) {
        // TODO(crbug.com/385987224): Do not open a popup again when the user clicks the action
        // button while its popup is open.
        closePopup();

        ExtensionActionPopupContents contents = ExtensionActionPopupContents.create(nativeHostPtr);

        View buttonView = getButtonViewForId(actionId);
        if (buttonView == null) {
            return;
        }

        assert mCurrentPopup == null;
        mCurrentPopup =
                new ExtensionActionPopup(mContext, mWindowAndroid, buttonView, actionId, contents);
        mCurrentPopup.loadInitialPage();
        mCurrentPopup.addOnDismissListener(this::closePopup);
        mCurrentPopupActionId = actionId;
    }

    @Nullable
    private View getButtonViewForId(String actionId) {
        for (int i = 0; i < mModels.size(); i++) {
            PropertyModel model = mModels.get(i).model;
            if (actionId.equals(model.get(ExtensionActionButtonProperties.ID))) {
                return mContainer.getChildAt(i);
            }
        }
        return null;
    }

    private void onContextClick(ListMenuButton buttonView, String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return;
        }

        ExtensionActionContextMenuBridge bridge =
                new ExtensionActionContextMenuBridge(
                        mTask, actionId, webContents, ContextMenuSource.TOOLBAR_ACTION);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext, buttonView, bridge, MenuBuilderHelper.getRectProvider(buttonView), null);
    }

    private void closePopup() {
        if (mCurrentPopup == null) {
            return;
        }

        // Clear mCurrentPopup now to avoid calling closePopup recursively via OnDismissListener.
        ExtensionActionPopup popup = mCurrentPopup;
        mCurrentPopup = null;
        popup.destroy();
        mCurrentPopupActionId = null;
    }

    private class ToolbarObserver implements ExtensionsToolbarBridge.Observer {
        @Override
        public void onActionsInitialized() {
            reconcileActionItems();
        }

        @Override
        public void onActionAdded(String actionId) {
            reconcileActionItems();
        }

        @Override
        public void onActionRemoved(String actionId) {
            removeActionItem(actionId);
        }

        @Override
        public void onActionUpdated(String actionId) {
            updateActionProperties(actionId);
        }

        @Override
        public void onPinnedActionsChanged() {
            reconcileActionItems();
        }
    }

    private class ToolbarDelegate implements ExtensionsToolbarBridge.Delegate {
        @Override
        public void triggerPopup(String actionId, long nativeHostPtr) {
            ExtensionActionListMediator.this.triggerPopup(actionId, nativeHostPtr);
        }
    }
}
