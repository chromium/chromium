// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction.HoverCardState;
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
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

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

        /** State when popup is waiting for UI animations to finish. */
        public static final class PopupPending extends ActionState {
            private final String mActionId;
            private final ExtensionActionPopupContents mContents;

            public PopupPending(String actionId, ExtensionActionPopupContents contents) {
                mActionId = actionId;
                mContents = contents;
            }

            public String getActionId() {
                return mActionId;
            }

            public ExtensionActionPopupContents getContents() {
                return mContents;
            }
        }

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
    private final @Nullable ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    private final @Nullable SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
    private final ExtensionActionListCoordinator.RecyclerViewDelegate mRecyclerViewDelegate;
    private final TabModelSelector mTabModelSelector;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private @Nullable AnchoredPopupWindow mHoverCard;
    private @Nullable String mHoverCardActionId;
    private @Nullable Runnable mShowHoverCardRunnable;

    private final ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private final ToolbarDelegate mToolbarDelegate = new ToolbarDelegate();
    private final ToolbarObserver mToolbarObserver = new ToolbarObserver();

    private ActionState mActionState = new ActionState.Idle();

    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    // The ID of the action that should be "popped out", since it needs to be visible to show a
    // popup anchored to the action view (e.g extension popup or context menu). During animation, it
    // should reflect the end state.
    @Nullable private String mPoppedOutActionId;

    // Whether the toolbar has allocated us with enough width to show the popped out action. Until
    // then, we assume we have infinite space.
    private boolean mCanShowPoppedOutAction = true;

    // The maximum width that the icons (other than popped out action) can take up. It is set when
    // the toolbar requests us to be a certain size. Until then, we assume we have infinite space.
    private @Nullable Integer mAvailableWidthForPinnedActions;

    public ExtensionActionListMediator(
            Context context,
            WindowAndroid windowAndroid,
            ModelList models,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionActionListCoordinator.RecyclerViewDelegate recyclerViewDelegate,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            TabModelSelector tabModelSelector) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mModels = models;
        mTask = task;
        mProfile = profile;
        mCurrentTabSupplier = currentTabSupplier;
        mRecyclerViewDelegate = recyclerViewDelegate;
        mExtensionsToolbarBridge = extensionsToolbarBridge;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mSelectionDropdownMenuDelegate = selectionDropdownMenuDelegate;
        mTabModelSelector = tabModelSelector;

        mExtensionsToolbarBridge.setActionListDelegate(mToolbarDelegate);
        mExtensionsToolbarBridge.addObserver(mToolbarObserver);
        reconcileActionItems();
    }

    @Override
    public void destroy() {
        if (mShowHoverCardRunnable != null) {
            mHandler.removeCallbacks(mShowHoverCardRunnable);
            mShowHoverCardRunnable = null;
        }

        mRecyclerViewDelegate.clearOnAnimationsFinishedRunnables();

        closeHoverCard();
        closePopup();
        closeContextMenu();

        assert mActionState instanceof ActionState.Idle;

        mExtensionsToolbarBridge.removeObserver(mToolbarObserver);
        mExtensionsToolbarBridge.setActionListDelegate(null);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /** Returns whether there is an action that is popped out. */
    public boolean hasPoppedOutAction() {
        return mPoppedOutActionId != null;
    }

    /**
     * Remembers whether we can show the popped out action, but does not update the UI just yet, to
     * avoid refreshing the UI twice. We actually update the UI via {@link fitActionsWithinWidth()},
     * which will be called due to the rest of the action list having a lower priority for the
     * toolbar to display, as determined in {@code ToolbarUtils}.
     *
     * @param availableWidth The maximum width that toolbar allows us to use up.
     * @return The actual width that we will use up for the popped out action.
     */
    public int setCanShowPoppedOutAction(int availableWidth) {
        int itemWidth = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        if (hasPoppedOutAction() && itemWidth <= availableWidth) {
            mCanShowPoppedOutAction = true;
            return itemWidth;
        } else {
            mCanShowPoppedOutAction = false;
            return 0;
        }
    }

    /**
     * Reconciles the current list of models with the list of IDs from the bridge. This handles
     * additions, removals, and reordering without rebuilding the whole list.
     */
    @VisibleForTesting
    void reconcileActionItems() {
        // The pinned action IDs are a subset of all action IDs.
        Set<String> allActionIdsSet =
                new HashSet<>(Arrays.asList(mExtensionsToolbarBridge.getAllActionIds()));
        String[] pinnedActionIds = mExtensionsToolbarBridge.getPinnedActionIds();

        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;

        @Nullable String currentPopupActionId = null;
        if (mActionState instanceof ActionState.PopupActive activeState) {
            currentPopupActionId = activeState.getActionId();
        }

        // Optimization: Remove items that are no longer present in the new list.
        // This prevents unnecessary moves if the first item is removed.
        Set<String> pinnedActionIdsSet = new HashSet<>(Arrays.asList(pinnedActionIds));
        for (int i = mModels.size() - 1; i >= 0; i--) {
            String id = getActionIdForIndex(i);

            boolean isPoppedOutAction = mPoppedOutActionId != null && mPoppedOutActionId.equals(id);
            if (pinnedActionIdsSet.contains(id) || isPoppedOutAction) {
                // We shouldn't remove the model if the icon is pinned or popped out.
                continue;
            }

            if (id.equals(currentPopupActionId)) {
                closePopup();
            }
            mModels.removeAt(i);
        }

        int itemWidth = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);

        int maxNumberOfItems = Integer.MAX_VALUE;
        if (mAvailableWidthForPinnedActions != null) {
            assert itemWidth > 0;
            maxNumberOfItems = mAvailableWidthForPinnedActions / itemWidth;
        }

        // O(N) for removals/no-ops; O(N^2) for reordering/insertions.
        int currentModelIndex = 0;

        // Go through non-popped-out actions.
        for (String actionId : pinnedActionIds) {
            if (mPoppedOutActionId != null && mPoppedOutActionId.equals(actionId)) {
                continue;
            }

            if (currentModelIndex >= maxNumberOfItems) {
                // We ran out of space.
                break;
            }

            currentModelIndex =
                    reconcileItem(
                            actionId,
                            currentModelIndex,
                            webContents,
                            /* isPoppedOut= */ false,
                            allActionIdsSet);
        }

        // Deal with the popped out action last, as it should appear on the [right|left] end of the
        // list for [LTR|RTL].
        if (mPoppedOutActionId != null
                && allActionIdsSet.contains(mPoppedOutActionId)
                && mCanShowPoppedOutAction) {
            currentModelIndex =
                    reconcileItem(
                            mPoppedOutActionId,
                            currentModelIndex,
                            webContents,
                            /* isPoppedOut= */ true,
                            allActionIdsSet);
        }

        // Remove rest of the items.
        while (mModels.size() > currentModelIndex) {
            if (getActionIdForIndex(currentModelIndex).equals(currentPopupActionId)) {
                closePopup();
            }
            mModels.removeAt(currentModelIndex);
        }
    }

    /**
     * Helper to calculate whether we should show an action item, and if so to reorder {@link
     * mModels} so that {@code actionId} comes at {@code currentIndex}.
     *
     * @param actionId The ID of the action in question.
     * @param currentIndex The current index of the action in {@link mModels}.
     * @param webContents The WebContents to use.
     * @param isPoppedOut Whether the action is shown because it's popped out.
     * @param allActionIds The list of all active actions, pinned or unpinned.
     * @return The next index of {@link mModels} that needs to be evaluated.
     */
    private int reconcileItem(
            String actionId,
            int currentIndex,
            @Nullable WebContents webContents,
            boolean isPoppedOut,
            Set<String> allActionIds) {
        assert allActionIds.contains(actionId);
        ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId, webContents);
        if (action == null) {
            return currentIndex;
        }

        if (currentIndex < mModels.size() && getActionIdForIndex(currentIndex).equals(actionId)) {
            // We already have {@link actionId} in the correct place. We can just move onto the next
            // one.
            return currentIndex + 1;
        }

        int indexInModels = findIndexForId(actionId, currentIndex + 1);
        if (indexInModels == -1) {
            mModels.add(currentIndex, createListItem(action, webContents, isPoppedOut));
        } else {
            mModels.move(indexInModels, currentIndex);
        }
        return currentIndex + 1;
    }

    /**
     * Returns a {@link ListItem} for a specific action.
     *
     * @param action The {@link ExtensionAction} of the action.
     * @param webContents The current {@link WebContents}.
     * @param isPoppedOut True if this action is displayed because it's popped out.
     */
    private ListItem createListItem(
            ExtensionAction action, @Nullable WebContents webContents, boolean isPoppedOut) {
        String actionId = action.getId();

        return new ListItem(
                ListItemType.EXTENSION_ACTION,
                new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                        .with(
                                ExtensionActionButtonProperties.ACCESSIBLE_NAME,
                                action.getAccessibleName())
                        .with(
                                ExtensionActionButtonProperties.ICON,
                                getIconForAction(actionId, webContents))
                        .with(ExtensionActionButtonProperties.ID, actionId)
                        .with(
                                ExtensionActionButtonProperties.IS_DRAGGABLE,
                                mExtensionsToolbarBridge.isActionDraggable(actionId)
                                        && !isPoppedOut)
                        .with(
                                ExtensionActionButtonProperties.ON_CLICK_LISTENER,
                                (view) -> onPrimaryClick(actionId))
                        .with(
                                ExtensionActionButtonProperties.ON_HOVER_LISTENER,
                                (view, event) -> {
                                    return onHover(actionId, event, webContents);
                                })
                        .with(
                                ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER,
                                (view) -> {
                                    requestShowContextMenu(actionId);
                                    return true;
                                })
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
    void updateActionProperties(String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;
        updateActionPropertiesWithWebContents(actionId, webContents);
    }

    // Updates model properties while keeping it in place.
    void updateActionPropertiesWithWebContents(String actionId, @Nullable WebContents webContents) {
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

        PropertyModel model = mModels.get(index).model;
        model.set(ExtensionActionButtonProperties.ICON, icon);
        model.set(
                ExtensionActionButtonProperties.IS_DRAGGABLE,
                mExtensionsToolbarBridge.isActionDraggable(actionId));
        model.set(ExtensionActionButtonProperties.ACCESSIBLE_NAME, action.getAccessibleName());
    }

    private void updateActionPropertiesForAll(WebContents webContents) {
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

    private boolean onHover(String actionId, MotionEvent event, @Nullable WebContents webContents) {
        if (!(mActionState instanceof ActionState.Idle)) {
            return false;
        }

        if (event.getAction() == MotionEvent.ACTION_HOVER_ENTER) {
            if (mShowHoverCardRunnable != null) {
                mHandler.removeCallbacks(mShowHoverCardRunnable);
            }

            if (mHoverCard == null) {
                mHoverCardActionId = actionId;
                mShowHoverCardRunnable =
                        () -> {
                            showHoverCard(actionId, webContents);
                            mShowHoverCardRunnable = null;
                        };
                mHandler.postDelayed(
                        mShowHoverCardRunnable, ViewConfiguration.getLongPressTimeout());
            } else if (!actionId.equals(mHoverCardActionId)) {
                closeHoverCard();
                showHoverCard(actionId, webContents);
            }
        } else if (event.getAction() == MotionEvent.ACTION_HOVER_EXIT) {
            if (actionId.equals(mHoverCardActionId)) {
                if (mShowHoverCardRunnable != null) {
                    mHandler.removeCallbacks(mShowHoverCardRunnable);
                    mShowHoverCardRunnable = null;
                }
                closeHoverCard();
            }
        }

        // We don't consume the event because we want the button to still be hovered.
        return false;
    }

    private void showHoverCard(String actionId, @Nullable WebContents webContents) {
        if (webContents == null) {
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            return;
        }

        ExtensionAction action = mExtensionsToolbarBridge.getAction(actionId, webContents);
        if (action == null) {
            return;
        }

        View anchorView = mRecyclerViewDelegate.getButtonViewForId(actionId);
        if (anchorView == null) {
            return;
        }

        HoverCardState state = action.getHoverCardState();
        RectProvider rectProvider = MenuBuilderHelper.getRectProvider(anchorView);

        View contentView =
                LayoutInflater.from(activity).inflate(R.layout.extension_action_hover_card, null);

        PropertyModel model =
                new PropertyModel.Builder(ExtensionActionHoverCardProperties.ALL_KEYS)
                        .with(ExtensionActionHoverCardProperties.ACTION_TITLE, action.getTitle())
                        .with(
                                ExtensionActionHoverCardProperties.SITE_ACCESS_TITLE,
                                state.getSiteAccessTitle())
                        .with(
                                ExtensionActionHoverCardProperties.SITE_ACCESS_DESC,
                                state.getSiteAccessDescription())
                        .with(ExtensionActionHoverCardProperties.POLICY_TEXT, state.getPolicyText())
                        .build();

        PropertyModelChangeProcessor.create(
                model, contentView, ExtensionActionHoverCardViewBinder::bind);

        mHoverCard =
                new AnchoredPopupWindow.Builder(
                                anchorView.getContext(),
                                anchorView.getRootView(),
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                rectProvider)
                        .setVerticalOverlapAnchor(false)
                        .setHorizontalOverlapAnchor(true)
                        .setMaxWidth(
                                anchorView
                                        .getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.extension_action_hover_card_width))
                        .setFocusable(false)
                        .setTouchable(false)
                        .setAnimateFromAnchor(false)
                        .setAnimationStyle(R.style.PopupWindowAnimFade)
                        .build();

        mHoverCard.show();
        mHoverCardActionId = actionId;
    }

    private void closeHoverCard() {
        if (mHoverCard != null) {
            mHoverCard.dismiss();
            mHoverCard = null;
            mHoverCardActionId = null;
        }
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
        executeUserAction(actionId, InvocationSource.TOOLBAR_BUTTON);
    }

    /**
     * Executes the given action.
     *
     * @param actionId The ID of the action to execute.
     * @param source How this execution was triggered.
     */
    public void executeUserAction(String actionId, @InvocationSource int source) {
        mExtensionsToolbarBridge.executeUserAction(actionId, source);
    }

    /**
     * Run {@code onVisible} after making sure that the action exists by popping out.
     *
     * @param actionId The ID of the action that needs visibility.
     * @param runnable The runnable to run after all animations end and the {@link RecyclerView}
     *     reaches a stable state. This does not guarantee that when this runnable is called the
     *     action is visible - for example, another animation might have taken over and unpopped the
     *     action.
     */
    public void requestActionVisibility(String actionId, Runnable runnable) {
        if (mPoppedOutActionId != null && !actionId.equals(mPoppedOutActionId)) {
            // Undo pop out for any other action.
            undoPopout();
        }

        mRecyclerViewDelegate.addOnAnimationsFinishedRunnable(runnable);

        if (findIndexForId(actionId) == -1) {
            mPoppedOutActionId = actionId;
        }

        // Force the toolbar to recalculate the toolbar ranking and provide us with a new {@code
        // availableWidth}, given that the amount we'll use has changed.
        // Also, when no animation is necessary (e.g. the action is already pinned), this will
        // trigger {@link ExtensionActionListRecyclerView}'s {@code onLayoutChangedListener} to
        // fire, calling the runnable that we just added.
        mRecyclerViewDelegate.requestLayoutWithViewUtils();
    }

    @VisibleForTesting
    void undoPopout() {
        if (mPoppedOutActionId != null) {
            mPoppedOutActionId = null;

            // Request layout to update available width and trigger updates.
            mRecyclerViewDelegate.requestLayoutWithViewUtils();
        }
    }

    private void requestShowPopup(String actionId, long nativeHostPtr) {
        closeHoverCard();
        closePopup();
        closeContextMenu();

        mActionState =
                new ActionState.PopupPending(
                        actionId, ExtensionActionPopupContents.create(nativeHostPtr));

        requestActionVisibility(actionId, () -> showPopupOnAnchor());
    }

    private void showPopupOnAnchor() {
        if (!(mActionState instanceof ActionState.PopupPending)) {
            return;
        }

        ActionState.PopupPending state = (ActionState.PopupPending) mActionState;
        String actionId = state.getActionId();
        ExtensionActionPopupContents contents = state.getContents();

        ListMenuButton buttonView =
                (ListMenuButton) mRecyclerViewDelegate.getButtonViewForId(actionId);
        if (buttonView == null) {
            contents.destroy();
            mActionState = new ActionState.Idle();
            undoPopout();
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            contents.destroy();
            mActionState = new ActionState.Idle();
            return;
        }

        // We set the button state to "pressed" before showing the popup, because we want it to have
        // the "pressed" look while loading the action popup too. For context menu {@link
        // ListMenuButton} already handles this, but not for popup where we show the window
        // ourselves.
        buttonView.setIsPressed(true);

        ExtensionActionPopup popup =
                new ExtensionActionPopup(
                        activity,
                        mWindowAndroid,
                        buttonView,
                        actionId,
                        contents,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        mTabModelSelector);
        popup.loadInitialPage();
        popup.addOnDismissListener(this::closePopup);
        mActionState = new ActionState.PopupActive(popup, actionId);
    }

    private void closePopup() {
        if (mActionState instanceof ActionState.PopupPending pendingState) {
            // Handle cancellation of a pending popup.
            pendingState.getContents().destroy();
            mActionState = new ActionState.Idle();
            undoPopout();
            return;
        }

        if (!(mActionState instanceof ActionState.PopupActive)) {
            return;
        }

        ActionState.PopupActive actionState = (ActionState.PopupActive) mActionState;

        // Clear state now to avoid calling closePopup recursively via OnDismissListener.
        mActionState = new ActionState.Idle();

        ExtensionActionPopup popup = actionState.getPopup();
        ListMenuButton buttonView =
                (ListMenuButton)
                        mRecyclerViewDelegate.getButtonViewForId(actionState.getActionId());

        popup.destroy();

        if (buttonView != null) {
            buttonView.setIsPressed(false);
        }

        undoPopout();
    }

    @VisibleForTesting
    void requestShowContextMenu(String actionId) {
        closeHoverCard();
        closePopup();
        closeContextMenu();

        requestActionVisibility(actionId, () -> showContextMenuOnAnchor(actionId));
    }

    private void showContextMenuOnAnchor(String actionId) {
        ListMenuButton buttonView =
                (ListMenuButton) mRecyclerViewDelegate.getButtonViewForId(actionId);
        if (buttonView == null) {
            undoPopout();
            return;
        }

        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            undoPopout();
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            undoPopout();
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
                        mRecyclerViewDelegate.getButtonViewForId(
                                ((ActionState.ContextMenuActive) mActionState).getActionId());
        if (buttonView != null) {
            // We expect the View to exist if {@code mCurrentContextMenuActionId} is non-null, but
            // {@link RecyclerView} may have already destroyed it. In this case, we don't need to
            // call {@link ListMenuButton#dismiss()} because {@link
            // ListMenuButton#onDetachedFromWindow()} calls it automatically.
            buttonView.dismiss();
        }

        mActionState = new ActionState.Idle();
        undoPopout();
    }

    /** Updates the list of displayed actions to fit within the provided width constraint. */
    public void fitActionsWithinWidth(int availableWidth) {
        mAvailableWidthForPinnedActions = availableWidth;

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
        public void onActiveWebContentsChanged(WebContents webContents) {
            updateActionPropertiesForAll(webContents);
        }
    }

    private class ToolbarDelegate implements ExtensionsToolbarBridge.ActionListDelegate {
        @Override
        public void triggerPopup(String actionId, long nativeHostPtr) {
            requestShowPopup(actionId, nativeHostPtr);
        }

        @Override
        public void showContextMenu(String actionId) {
            requestShowContextMenu(actionId);
        }

        @Override
        public boolean hasPoppedOutAction() {
            return ExtensionActionListMediator.this.hasPoppedOutAction();
        }

        @Override
        public void hideActivePopup() {
            ExtensionActionListMediator.this.closePopup();
        }

        @Override
        public boolean hasActivePopup() {
            return mActionState instanceof ActionState.PopupActive;
        }
    }
}
