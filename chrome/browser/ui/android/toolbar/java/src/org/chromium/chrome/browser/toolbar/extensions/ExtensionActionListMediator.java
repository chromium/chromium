// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
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

    /** A sealed class to guarantee that only either the popup or the context menu is open. */
    private abstract static sealed class ActionState {
        private ActionState() {}

        /** State when no menu or popup is active. */
        public static final class Idle extends ActionState {}

        /** State when a popup is active. */
        public static final class PopupActive extends ActionState {
            private final ExtensionActionPopup mPopup;
            private final String mActionId;

            public PopupActive(ExtensionActionPopup popup, String actionId) {
                mPopup = popup;
                mActionId = actionId;
            }

            public ExtensionActionPopup getPopup() {
                return mPopup;
            }

            public String getActionId() {
                return mActionId;
            }
        }

        /** State when a context menu is active. */
        public static final class ContextMenuActive extends ActionState {
            private final String mActionId;

            public ContextMenuActive(String actionId) {
                mActionId = actionId;
            }

            public String getActionId() {
                return mActionId;
            }
        }
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModels;
    private final ChromeAndroidTask mTask;
    private final Profile mProfile;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionActionListCoordinator.ActionAnchorViewProvider mActionAnchorViewProvider;
    private final @Nullable ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    private final @Nullable SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;

    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final ToolbarDelegate mToolbarDelegate = new ToolbarDelegate();
    private final ToolbarObserver mToolbarObserver = new ToolbarObserver();

    private ActionState mActionState = new ActionState.Idle();

    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    // The maximum width that the icons can take up. It is set when the toolbar requests us to be a
    // certain size. Until then, we assume we have infinite space.
    private @Nullable Integer mAvailableWidth;

    public ExtensionActionListMediator(
            Context context,
            WindowAndroid windowAndroid,
            ModelList models,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionActionListCoordinator.ActionAnchorViewProvider actionAnchorViewProvider,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModels = models;
        mTask = task;
        mProfile = profile;
        mCurrentTabSupplier = currentTabSupplier;
        mActionAnchorViewProvider = actionAnchorViewProvider;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mSelectionDropdownMenuDelegate = selectionDropdownMenuDelegate;

        mExtensionsToolbarBridge.setDelegate(mToolbarDelegate);
        mExtensionsToolbarBridge.addObserver(mToolbarObserver);
        reconcileActionItems();
    }

    @Override
    public void destroy() {
        closePopup();
        closeContextMenu();

        assert mActionState instanceof ActionState.Idle;

        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
        mExtensionsToolbarBridge.setDelegate(null);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * Reconciles the current list of models with the list of IDs from the bridge. This handles
     * additions, removals, and reordering without rebuilding the whole list.
     */
    @VisibleForTesting
    void reconcileActionItems() {
        String[] actionIds = mExtensionsToolbarBridge.getPinnedActionIds();

        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        @Nullable String currentPopupActionId = null;
        if (mActionState instanceof ActionState.PopupActive activeState) {
            currentPopupActionId = activeState.getActionId();
        }

        // Optimization: Remove items that are no longer present in the new list.
        // This prevents unnecessary moves if the first item is removed.
        Set<String> actionIdsSet = new HashSet<>(Arrays.asList(actionIds));
        for (int i = mModels.size() - 1; i >= 0; i--) {
            String id = getActionIdForIndex(i);
            if (!actionIdsSet.contains(id)) {
                if (id.equals(currentPopupActionId)) {
                    closePopup();
                }
                mModels.removeAt(i);
            }
        }

        int maxNumberOfItems = Integer.MAX_VALUE;
        if (mAvailableWidth != null) {
            int itemWidth =
                    mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            assert itemWidth > 0;
            maxNumberOfItems = mAvailableWidth / itemWidth;
        }

        // O(N) for removals/no-ops; O(N^2) for reordering/insertions.
        int currentModelIndex = 0;
        for (String actionId : actionIds) {
            if (currentModelIndex >= maxNumberOfItems) {
                break;
            }

            ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId, webContents);
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
            if (getActionIdForIndex(currentModelIndex).equals(currentPopupActionId)) {
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
                        .with(
                                ExtensionActionButtonProperties.ACCESSIBLE_NAME,
                                action.getAccessibleName())
                        .with(ExtensionActionButtonProperties.ICON, icon)
                        .with(ExtensionActionButtonProperties.ID, actionId)
                        .with(
                                ExtensionActionButtonProperties.ON_CLICK_LISTENER,
                                (view) -> onPrimaryClick(actionId))
                        .with(
                                ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER,
                                (view) -> {
                                    requestShowContextMenu(actionId);
                                    return true;
                                })
                        .with(ExtensionActionButtonProperties.TOOLTIP, action.getTooltip())
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

    // Updates model properties while keeping it in place.
    @VisibleForTesting
    void updateActionProperties(String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        int index = findIndexForId(actionId);
        if (index == -1) {
            return;
        }

        updateActionPropertiesForIndex(index, actionId, webContents);
    }

    private void updateActionPropertiesForIndex(
            int index, String actionId, @Nullable WebContents webContents) {
        ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId, webContents);
        if (action == null) {
            return;
        }

        Bitmap icon = getIconForAction(actionId, webContents);
        mModels.get(index).model.set(ExtensionActionButtonProperties.ICON, icon);

        mModels.get(index)
                .model
                .set(ExtensionActionButtonProperties.ACCESSIBLE_NAME, action.getAccessibleName());
        mModels.get(index).model.set(ExtensionActionButtonProperties.TOOLTIP, action.getTooltip());
    }

    private void updateActionPropertiesForAll() {
        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        for (int i = 0; i < mModels.size(); i++) {
            updateActionPropertiesForIndex(i, getActionIdForIndex(i), webContents);
        }
    }

    // Finds the model for {@code actionId} inside {@code mModels}, and returns the index if it
    // exists. If not, returns -1.
    private int findIndexForId(String actionId) {
        return findIndexForId(actionId, /* startIndex= */ 0);
    }

    // Finds the model for {@code actionId} inside {@code mModels} after {@code startIndex}, and
    // returns the index if it exists. If not, returns -1.
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
        if (mActionState instanceof ActionState.PopupActive activeState) {
            boolean closeOnly = activeState.getActionId().equals(actionId);
            closePopup();
            if (closeOnly) {
                return;
            }
        }
        if (mActionState instanceof ActionState.ContextMenuActive) {
            closeContextMenu();
            return;
        }
        mExtensionsToolbarBridge.executeUserAction(actionId, InvocationSource.TOOLBAR_BUTTON);
    }

    private void requestShowPopup(String actionId, long nativeHostPtr) {
        closePopup();
        closeContextMenu();

        ExtensionActionPopupContents contents = ExtensionActionPopupContents.create(nativeHostPtr);

        if (findIndexForId(actionId) == -1) {
            // TODO(crbug.com/483194547): Implement popping out actions.
            contents.destroy();
            return;
        } else {
            showPopupOnReadyAnchor(actionId, contents);
        }
    }

    private void showPopupOnReadyAnchor(String actionId, ExtensionActionPopupContents contents) {
        View buttonView = mActionAnchorViewProvider.getButtonViewForId(actionId);
        if (buttonView == null) {
            contents.destroy();
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            contents.destroy();
            return;
        }

        assert mActionState instanceof ActionState.Idle;
        ExtensionActionPopup popup =
                new ExtensionActionPopup(
                        activity,
                        mWindowAndroid,
                        buttonView,
                        actionId,
                        contents,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate);
        popup.loadInitialPage();
        popup.addOnDismissListener(this::closePopup);
        mActionState = new ActionState.PopupActive(popup, actionId);
    }

    private void closePopup() {
        if (!(mActionState instanceof ActionState.PopupActive)) {
            return;
        }

        // Clear the popup now to avoid calling closePopup recursively via OnDismissListener.
        ExtensionActionPopup popup = ((ActionState.PopupActive) mActionState).getPopup();

        mActionState = new ActionState.Idle();

        popup.destroy();
    }

    @VisibleForTesting
    void requestShowContextMenu(String actionId) {
        closePopup();
        closeContextMenu();

        if (findIndexForId(actionId) == -1) {
            // TODO(crbug.com/483194547): Implement popping out actions.
            return;
        } else {
            showContextMenuOnReadyAnchor(actionId);
        }
    }

    private void showContextMenuOnReadyAnchor(String actionId) {
        ListMenuButton buttonView =
                (ListMenuButton) mActionAnchorViewProvider.getButtonViewForId(actionId);
        if (buttonView == null) {
            return;
        }

        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return;
        }

        assert mActionState instanceof ActionState.Idle;
        ExtensionActionContextMenuBridge bridge =
                new ExtensionActionContextMenuBridge(
                        mTask, mProfile, actionId, webContents, ContextMenuSource.TOOLBAR_ACTION);
        ExtensionActionContextMenuUtils.showContextMenu(
                mContext,
                buttonView,
                bridge,
                MenuBuilderHelper.getRectProvider(buttonView),
                this::closeContextMenu);
        mActionState = new ActionState.ContextMenuActive(actionId);
    }

    private void closeContextMenu() {
        if (!(mActionState instanceof ActionState.ContextMenuActive)) {
            return;
        }

        ListMenuButton buttonView =
                (ListMenuButton)
                        mActionAnchorViewProvider.getButtonViewForId(
                                ((ActionState.ContextMenuActive) mActionState).getActionId());
        if (buttonView != null) {
            // We expect the View to exist if {@code mCurrentContextMenuActionId} is non-null, but
            // {@link RecyclerView} may have already destroyed it. In this case, we don't need to
            // call {@link ListMenuButton#dismiss()} because {@link
            // ListMenuButton#onDetachedFromWindow()} calls it automatically.
            buttonView.dismiss();
        }

        mActionState = new ActionState.Idle();
    }

    /** Updates the list of displayed actions to fit within the provided width constraint. */
    public void fitActionsWithinWidth(int availableWidth) {
        mAvailableWidth = availableWidth;

        // If this is called during an animation (e.g. the user resizes window during pinning /
        // unpinning animation), we abandon the animation and update to the new state instantly.
        reconcileActionItems();
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
            reconcileActionItems();
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
            ExtensionActionListMediator.this.requestShowPopup(actionId, nativeHostPtr);
        }
    }
}
