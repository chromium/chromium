// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
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
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

@NullMarked
class ExtensionActionListMediator implements Destroyable {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModels;
    private final ChromeAndroidTask mTask;
    private final Profile mProfile;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionActionListRecyclerView mContainer;

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
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionActionListRecyclerView container,
            ExtensionsToolbarBridge extensionsToolbarBridge) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModels = models;
        mTask = task;
        mProfile = profile;
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

        // Optimization: Remove items that are no longer present in the new list.
        // This prevents unnecessary moves if the first item is removed.
        Set<String> actionIdsSet = new HashSet<>(Arrays.asList(actionIds));
        for (int i = mModels.size() - 1; i >= 0; i--) {
            String id = getActionIdForIndex(i);
            if (!actionIdsSet.contains(id)) {
                if (id.equals(mCurrentPopupActionId)) {
                    closePopup();
                }
                mModels.removeAt(i);
            }
        }

        // O(N) for removals/no-ops; O(N^2) for reordering/insertions.
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
                                ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER,
                                (view) -> {
                                    onContextClick(actionId);
                                    return true;
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

        updateActionPropertiesForIndex(index, actionId, webContents);
    }

    private void updateActionPropertiesForIndex(
            int index, String actionId, @Nullable WebContents webContents) {
        ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId);
        if (action == null) {
            return;
        }

        Bitmap icon = getIconForAction(actionId, webContents);
        mModels.get(index).model.set(ExtensionActionButtonProperties.ICON, icon);

        mModels.get(index).model.set(ExtensionActionButtonProperties.TITLE, action.getTitle());
    }

    private void updateActionPropertiesForAll() {
        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        for (int i = 0; i < mModels.size(); i++) {
            updateActionPropertiesForIndex(i, getActionIdForIndex(i), webContents);
        }
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
            contents.destroy();
            return;
        }

        assert mCurrentPopup == null;
        mCurrentPopup =
                new ExtensionActionPopup(mContext, mWindowAndroid, buttonView, actionId, contents);
        mCurrentPopup.loadInitialPage();
        mCurrentPopup.addOnDismissListener(this::closePopup);
        mCurrentPopupActionId = actionId;
    }

    @VisibleForTesting
    @Nullable View getButtonViewForId(String actionId) {
        for (int i = 0; i < mModels.size(); i++) {
            PropertyModel model = mModels.get(i).model;
            if (actionId.equals(model.get(ExtensionActionButtonProperties.ID))) {
                RecyclerView.ViewHolder holder = mContainer.findViewHolderForAdapterPosition(i);

                if (holder == null) {
                    // TODO(crbug.com/478113313): If the action is unpinned, pop it out to show
                    // action popup.
                    return null;
                }

                return holder.itemView;
            }
        }
        return null;
    }

    private void onContextClick(String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return;
        }

        ListMenuButton buttonView = (ListMenuButton) getButtonViewForId(actionId);
        if (buttonView == null) {
            return;
        }

        ExtensionActionContextMenuBridge bridge =
                new ExtensionActionContextMenuBridge(
                        mTask, mProfile, actionId, webContents, ContextMenuSource.TOOLBAR_ACTION);

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

    /**
     * Communicates the move to the native side via the bridge to commit.
     *
     * @param targetIndex The new index of the moved action item.
     */
    public void onActionsSwapped(int targetIndex) {
        mExtensionsToolbarBridge.movePinnedAction(
                mModels.get(targetIndex).model.get(ExtensionActionButtonProperties.ID),
                targetIndex);
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

        @Override
        public void onActiveWebContentsChanged() {
            updateActionPropertiesForAll();
        }
    }

    private class ToolbarDelegate implements ExtensionsToolbarBridge.Delegate {
        @Override
        public void triggerPopup(String actionId, long nativeHostPtr) {
            ExtensionActionListMediator.this.triggerPopup(actionId, nativeHostPtr);
        }
    }
}
