// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.UiUtils.INVALID_TASK_ID;
import static org.chromium.components.browser_ui.widget.ListItemBuilder.buildSimpleMenuItem;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.util.TimeTextResolver;
import org.chromium.components.browser_ui.widget.BoundedLinearLayout;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Coordinator to construct the instance switcher dialog. TODO: Resolve various inconsistencies that
 * can be caused by Ui from multiple instances.
 */
@NullMarked
public class InstanceSwitcherCoordinator {
    // Last switcher dialog instance. This is used to prevent the user from interacting with
    // multiple instances of switcher UI.
    @SuppressLint("StaticFieldLeak")
    static @Nullable InstanceSwitcherCoordinator sPrevInstance;

    private static final int TYPE_INSTANCE = 0;

    private final Context mContext;
    private final InstanceSwitcherActionsDelegate mDelegate;
    private final ModalDialogManager mModalDialogManager;
    private final int mMaxInstanceCount;
    private final int mMinCommandItemHeightPx;
    private final int mItemPaddingHeightPx;

    private final ModelList mActiveModelList = new ModelList();
    private final ModelList mInactiveModelList = new ModelList();
    private final UiUtils mUiUtils;
    private final View mDialogView;
    private final boolean mIsIncognitoWindow;
    private final TabLayout mTabHeaderRow;

    private @Nullable PropertyModel mDialog;
    private @Nullable InstanceInfo mItemToDelete;
    private final LinearLayout mNewWindowLayout;
    private final TextView mMaxInfoView;
    private final HashSet<Integer> mSelectedItems;
    private boolean mNewWindowEnabled;
    private boolean mIsInactiveListShowing;
    private final FrameLayout mInstanceListContainer;
    private final FrameLayout mInactiveListEmptyStateView;
    private final RecyclerView mActiveInstancesList;
    private final RecyclerView mInactiveInstancesList;
    private final DialogListItemDecoration mActiveListItemDecoration;

    /**
     * Show instance switcher modal dialog UI.
     *
     * @param context Context to use to build the dialog.
     * @param modalDialogManager {@link ModalDialogManager} object.
     * @param iconBridge An object that fetches favicons from local DB.
     * @param delegate A delegate interface to handle instance switcher actions.
     * @param maxInstanceCount The maximum number of instances whose state can be persisted.
     * @param instanceInfo List of {@link InstanceInfo} for available Chrome instances.
     * @param isIncognitoWindow Used to determine if dialog should show "New window" or "New
     *     Incognito window".
     */
    public static void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            InstanceSwitcherActionsDelegate delegate,
            int maxInstanceCount,
            List<InstanceInfo> instanceInfo,
            boolean isIncognitoWindow) {
        new InstanceSwitcherCoordinator(
                        context,
                        modalDialogManager,
                        iconBridge,
                        delegate,
                        maxInstanceCount,
                        isIncognitoWindow)
                .show(instanceInfo);
    }

    private InstanceSwitcherCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            InstanceSwitcherActionsDelegate delegate,
            int maxInstanceCount,
            boolean isIncognitoWindow) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mUiUtils = new UiUtils(mContext, iconBridge);
        mDelegate = delegate;
        mMaxInstanceCount = maxInstanceCount;
        mMinCommandItemHeightPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.instance_switcher_dialog_list_item_height);
        mItemPaddingHeightPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.instance_switcher_dialog_list_item_padding);
        mIsIncognitoWindow = isIncognitoWindow;
        mSelectedItems = new HashSet<>();

        var activeListAdapter = getInstanceListAdapter(/* active= */ true);
        var inactiveListAdapter = getInstanceListAdapter(/* active= */ false);

        mDialogView = LayoutInflater.from(context).inflate(R.layout.instance_switcher_dialog, null);
        int screenSize =
                mContext.getResources().getConfiguration().screenLayout
                        & Configuration.SCREENLAYOUT_SIZE_MASK;
        boolean isFullScreen = screenSize < Configuration.SCREENLAYOUT_SIZE_LARGE;
        ((BoundedLinearLayout) mDialogView).setIgnoreConstraints(false, isFullScreen);

        mTabHeaderRow = mDialogView.findViewById(R.id.tabs);
        mInstanceListContainer = mDialogView.findViewById(R.id.instance_list_container);
        mMaxInfoView = mDialogView.findViewById(R.id.max_instance_info);
        mInactiveListEmptyStateView = mDialogView.findViewById(R.id.inactive_list_empty_state_view);
        mNewWindowLayout = mDialogView.findViewById(R.id.new_window);
        TextView newWindowTextView = mNewWindowLayout.findViewById(R.id.new_window_text);
        if (mIsIncognitoWindow) {
            newWindowTextView.setText(R.string.menu_new_incognito_window);
        }

        mActiveListItemDecoration = new DialogListItemDecoration(mItemPaddingHeightPx);
        var inactiveListItemDecoration = new DialogListItemDecoration(mItemPaddingHeightPx);

        mActiveInstancesList = mDialogView.findViewById(R.id.active_instance_list);
        mActiveInstancesList.setLayoutManager(
                new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
        mActiveInstancesList.setAdapter(activeListAdapter);
        mActiveInstancesList.addItemDecoration(mActiveListItemDecoration);

        mInactiveInstancesList = mDialogView.findViewById(R.id.inactive_instance_list);
        mInactiveInstancesList.setLayoutManager(
                new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
        mInactiveInstancesList.setAdapter(inactiveListAdapter);
        mInactiveInstancesList.addItemDecoration(inactiveListItemDecoration);

        addLayoutListeners(
                mDialogView,
                mTabHeaderRow,
                mInstanceListContainer,
                mActiveInstancesList,
                mInactiveInstancesList,
                () -> mIsInactiveListShowing,
                mNewWindowLayout,
                mMinCommandItemHeightPx,
                mItemPaddingHeightPx,
                /* registerResizeListener= */ true);

        mTabHeaderRow.addOnTabSelectedListener(
                new OnTabSelectedListener() {
                    @Override
                    public void onTabSelected(Tab tab) {
                        boolean isActiveTab = tab.getPosition() == 0;
                        mIsInactiveListShowing = !isActiveTab;
                        // Set params early using heuristic to prevent jank when switching tabs
                        preemptInstanceListContainerParamsUpdate(
                                mInstanceListContainer,
                                mInactiveInstancesList,
                                mIsInactiveListShowing,
                                shouldAllowNewWindowCreation());
                        mActiveInstancesList.setVisibility(isActiveTab ? View.VISIBLE : View.GONE);
                        mInactiveInstancesList.setVisibility(
                                isActiveTab ? View.GONE : View.VISIBLE);
                        addLayoutListeners(
                                mDialogView,
                                mTabHeaderRow,
                                mInstanceListContainer,
                                mActiveInstancesList,
                                mInactiveInstancesList,
                                () -> mIsInactiveListShowing,
                                mNewWindowLayout,
                                mMinCommandItemHeightPx,
                                mItemPaddingHeightPx,
                                /* registerResizeListener= */ false);
                        updateCommandUiState(shouldAllowNewWindowCreation());
                        unselectItems(/* hideVisibleList= */ false);
                        updateMoreMenu();
                        updatePositiveButtonText();
                        updateInactiveListEmptyStateVisibility();
                    }

                    @Override
                    public void onTabUnselected(Tab tab) {}

                    @Override
                    public void onTabReselected(Tab tab) {}
                });
    }

    private SimpleRecyclerViewAdapter getInstanceListAdapter(boolean active) {
        var adapter = new SimpleRecyclerViewAdapter(active ? mActiveModelList : mInactiveModelList);
        adapter.registerType(
                TYPE_INSTANCE,
                parentView ->
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.instance_switcher_item, null),
                InstanceSwitcherItemViewBinder::bind);
        return adapter;
    }

    // Adds listeners to layout the command item correctly relative to the instance list view.
    /* package */ static OnGlobalLayoutListener addLayoutListeners(
            View dialogView,
            TabLayout tabHeaderRow,
            View instanceListContainer,
            RecyclerView activeInstancesList,
            RecyclerView inactiveInstancesList,
            BooleanSupplier isInactiveListShowingSupplier,
            View newWindowLayout,
            int minCommandItemHeightPx,
            int itemPaddingHeightPx,
            boolean registerResizeListener) {
        if (registerResizeListener) {
            // Needed for proper placement of +New window item after a window height resize.
            // The global layout listener is one-shot so it is unable to handle subsequent window
            // resizes.
            dialogView.addOnLayoutChangeListener(
                    (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                        if (bottom - top != oldBottom - oldTop) {
                            maybeUpdateInstanceListContainerParams(
                                    dialogView,
                                    tabHeaderRow,
                                    instanceListContainer,
                                    activeInstancesList,
                                    inactiveInstancesList,
                                    isInactiveListShowingSupplier.getAsBoolean(),
                                    newWindowLayout,
                                    minCommandItemHeightPx,
                                    itemPaddingHeightPx);
                        }
                    });
        }

        var listener =
                new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        instanceListContainer
                                .getViewTreeObserver()
                                .removeOnGlobalLayoutListener(this);
                        maybeUpdateInstanceListContainerParams(
                                dialogView,
                                tabHeaderRow,
                                instanceListContainer,
                                activeInstancesList,
                                inactiveInstancesList,
                                isInactiveListShowingSupplier.getAsBoolean(),
                                newWindowLayout,
                                minCommandItemHeightPx,
                                itemPaddingHeightPx);
                    }
                };
        instanceListContainer.getViewTreeObserver().addOnGlobalLayoutListener(listener);
        return listener;
    }

    private static void maybeUpdateInstanceListContainerParams(
            View dialogView,
            TabLayout tabHeaderRow,
            View instanceListContainer,
            RecyclerView activeInstancesList,
            RecyclerView inactiveInstancesList,
            boolean isInactiveListShowing,
            View newWindowLayout,
            int minCommandItemHeightPx,
            int itemPaddingHeightPx) {
        int nonLastItemHeightPx = minCommandItemHeightPx + itemPaddingHeightPx;
        int activeListItemCount = assumeNonNull(activeInstancesList.getAdapter()).getItemCount();
        int inactiveListItemCount =
                assumeNonNull(inactiveInstancesList.getAdapter()).getItemCount();
        // We always add minCommandItemHeight even though +New Window may not always be shown
        // because if it isn't shown, the instance switcher will expand to the max height
        // anyways, making this calculation inapplicable.
        int activeListHeightPx = activeListItemCount * nonLastItemHeightPx + minCommandItemHeightPx;
        // The padding decoration is not applied to the last item of each list
        int inactiveListHeightPx =
                (inactiveListItemCount - 1) * nonLastItemHeightPx + minCommandItemHeightPx;
        int maxListHeightPx = Math.max(activeListHeightPx, inactiveListHeightPx);
        int overheadPx =
                tabHeaderRow.getMeasuredHeight() + dialogView.getPaddingTop() + itemPaddingHeightPx;

        int maxDialogHeightPx = maxListHeightPx + overheadPx;
        if (dialogView.getMinimumHeight() != maxDialogHeightPx) {
            dialogView.setMinimumHeight(maxDialogHeightPx);
        }

        boolean shouldFillVerticalSpace;

        if (isInactiveListShowing) {
            shouldFillVerticalSpace = inactiveListItemCount != 0;
        } else {
            boolean isActiveListScrollable =
                    activeInstancesList.getMeasuredHeight()
                            < activeInstancesList.computeVerticalScrollRange();
            int newWindowLayoutHeight =
                    (newWindowLayout != null && newWindowLayout.getVisibility() == View.VISIBLE)
                            ? newWindowLayout.getMeasuredHeight()
                            : 0;
            boolean isCommandLayoutCompressed = newWindowLayoutHeight < minCommandItemHeightPx;
            shouldFillVerticalSpace = isActiveListScrollable || isCommandLayoutCompressed;
        }

        applyVerticalSpaceParams(instanceListContainer, shouldFillVerticalSpace);
    }

    private static void preemptInstanceListContainerParamsUpdate(
            View instanceListContainer,
            RecyclerView inactiveInstancesList,
            boolean isInactiveListShowing,
            boolean newWindowEnabled) {
        boolean shouldFillVerticalSpace;

        if (isInactiveListShowing) {
            int inactiveListItemCount =
                    assumeNonNull(inactiveInstancesList.getAdapter()).getItemCount();
            shouldFillVerticalSpace = inactiveListItemCount != 0;
        } else {
            shouldFillVerticalSpace = !newWindowEnabled;
        }

        applyVerticalSpaceParams(instanceListContainer, shouldFillVerticalSpace);
    }

    private static void applyVerticalSpaceParams(
            View instanceListContainer, boolean shouldFillVerticalSpace) {
        LayoutParams params = (LayoutParams) instanceListContainer.getLayoutParams();

        // Optimization: Do nothing if the parameters are already in the correct state.
        if ((shouldFillVerticalSpace && params.weight == 1f)
                || (!shouldFillVerticalSpace && params.weight == 0)) {
            return;
        }

        if (shouldFillVerticalSpace) {
            // Configuration 1: Fill Vertical Space
            // This forces the container to expand and occupy all vertical space in the dialog.
            // When used:
            // - For long, scrollable lists: It ensures the RecyclerView takes up max space while
            //   pushing the command item to the very bottom (sticky footer).
            // - For the populated inactive list: It maintains the expected scrolling behavior.
            params.weight = 1f;
            params.height = 0;
        } else {
            // Configuration 2: Wrap Content
            // This allows the container to shrink and only take up as much height as its items.
            // When used:
            // - For short active lists: It ensures the "New Window" button sits directly below
            //   the last item, preventing a large empty gap between the list and the button.
            // - For the empty inactive list: It ensures the "No inactive windows" message sits
            //   right below the tabs.
            params.weight = 0f;
            params.height = LayoutParams.WRAP_CONTENT;
        }
        instanceListContainer.setLayoutParams(params);
    }

    private void show(List<InstanceInfo> items) {
        UiUtils.closeOpenDialogs();
        sPrevInstance = this;

        List<InstanceInfo> activeInstances = new ArrayList<>();
        List<InstanceInfo> inactiveInstances = new ArrayList<>();

        InstanceInfo currentInstance = null;
        for (int i = 0; i < items.size(); ++i) {
            InstanceInfo instanceInfo = items.get(i);
            // Add the current instance to the front of the list after it is sorted by timestamp.
            if (instanceInfo.type == InstanceInfo.Type.CURRENT) {
                currentInstance = instanceInfo;
                continue;
            }

            // An active instance should have an associated live task.
            boolean isActiveInstance = instanceInfo.taskId != INVALID_TASK_ID;
            if (isActiveInstance) {
                activeInstances.add(instanceInfo);
            } else {
                inactiveInstances.add(instanceInfo);
            }
        }

        activeInstances.sort(
                (info1, info2) -> Long.compare(info2.lastAccessedTime, info1.lastAccessedTime));
        assertNonNull(currentInstance);
        activeInstances.add(0, currentInstance);
        inactiveInstances.sort(
                (info1, info2) ->
                        Long.compare(calculateClosureTime(info2), calculateClosureTime(info1)));

        addItemsToModelList(mActiveModelList, activeInstances);
        addItemsToModelList(mInactiveModelList, inactiveInstances);

        // Update UI state.
        updateCommandUiState(shouldAllowNewWindowCreation());
        updateTabTitle(mActiveModelList.size(), mInactiveModelList.size());

        mDialog = createDialog(mDialogView);
        updateMoreMenu();
        updateActionButtons();
        mModalDialogManager.showDialog(mDialog, ModalDialogType.APP);
    }

    private void addItemsToModelList(ModelList modelList, List<InstanceInfo> items) {
        for (int i = 0; i < items.size(); ++i) {
            PropertyModel itemModel = generateListItem(items.get(i));
            modelList.add(new ListItem(TYPE_INSTANCE, itemModel));
        }
    }

    private static long calculateClosureTime(InstanceInfo item) {
        // Default to lastAccessedTime if closureTime is not available.
        return item.closureTime > 0 ? item.closureTime : item.lastAccessedTime;
    }

    private PropertyModel createDialog(View dialogView) {
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        sPrevInstance = null;
                    }

                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        switch (buttonType) {
                            case ModalDialogProperties.ButtonType.NEGATIVE:
                                dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                                break;
                            case ModalDialogProperties.ButtonType.POSITIVE:
                                assert mSelectedItems.size() == 1;
                                Integer selectedItem = mSelectedItems.iterator().next();
                                String userAction =
                                        mIsInactiveListShowing
                                                ? "Android.WindowManager.OpenInactiveWindow"
                                                : "Android.WindowManager.OpenActiveWindow";
                                RecordUserAction.record(userAction);
                                dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
                                mDelegate.openInstance(selectedItem);
                        }
                    }
                };
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.instance_switcher_header);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);

        builder.with(
                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                resources,
                mIsInactiveListShowing ? R.string.restore : R.string.open);
        builder.with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);

        if (UiUtils.isRobustWindowManagementBulkCloseEnabled()) {
            builder.with(
                    ModalDialogProperties.TITLE_BACK_BUTTON_CLICK_LISTENER,
                    v -> {
                        unselectItems(/* hideVisibleList= */ true);
                    });

            buildWindowManagerMoreMenu(builder);
        }
        return builder.build();
    }

    private PropertyModel generateListItem(InstanceInfo item) {
        String title = UiUtils.getItemTitle(mContext, item);
        String desc = mUiUtils.getItemDesc(item);
        boolean isCurrentWindow = item.type == InstanceInfo.Type.CURRENT;
        PropertyModel.Builder builder =
                new PropertyModel.Builder(InstanceSwitcherItemProperties.ALL_KEYS)
                        .with(InstanceSwitcherItemProperties.TITLE, title)
                        .with(InstanceSwitcherItemProperties.DESC, desc)
                        .with(InstanceSwitcherItemProperties.INSTANCE_ID, item.instanceId)
                        .with(
                                InstanceSwitcherItemProperties.CLICK_LISTENER,
                                (view) -> {
                                    selectInstance(item);
                                    updateMoreMenu();
                                });

        long timestamp;
        if (item.taskId != INVALID_TASK_ID) {
            buildMoreMenu(builder, item);
            timestamp = item.lastAccessedTime;
        } else {
            builder.with(
                    InstanceSwitcherItemProperties.CLOSE_BUTTON_CLICK_LISTENER,
                    (view) -> closeWindow(item));
            builder.with(InstanceSwitcherItemProperties.CLOSE_BUTTON_ENABLED, true);
            builder.with(
                    InstanceSwitcherItemProperties.CLOSE_BUTTON_CONTENT_DESCRIPTION,
                    mContext.getString(
                            R.string.instance_switcher_item_close_content_description,
                            UiUtils.getItemTitle(mContext, item)));
            timestamp = calculateClosureTime(item);
        }
        String lastAccessedString =
                isCurrentWindow
                        ? mContext.getString(R.string.instance_last_accessed_current)
                        : TimeTextResolver.resolveTimeAgoText(mContext.getResources(), timestamp);
        builder.with(InstanceSwitcherItemProperties.LAST_ACCESSED, lastAccessedString);
        builder.with(InstanceSwitcherItemProperties.IS_SELECTED, false);

        PropertyModel model = builder.build();
        mUiUtils.setFavicon(model, InstanceSwitcherItemProperties.FAVICON, item);
        return model;
    }

    private void buildMoreMenu(PropertyModel.Builder builder, InstanceInfo item) {
        ModelList moreMenu = new ModelList();
        if (UiUtils.isRobustWindowManagementEnabled()) {
            moreMenu.add(buildSimpleMenuItem(R.string.instance_switcher_name_window));
        }
        moreMenu.add(buildSimpleMenuItem(R.string.close));

        ListMenu.Delegate moreMenuDelegate =
                (model, view) -> {
                    int textId = model.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.instance_switcher_close_window
                            || textId == R.string.close) {
                        closeWindow(item);
                    }
                    if (textId == R.string.instance_switcher_name_window) {
                        showNameWindowDialog(item);
                    }
                };
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(mContext, moreMenu, moreMenuDelegate);
        listMenu.addContentViewClickRunnable(
                () -> {
                    RecordUserAction.record("Android.WindowManager.SecondaryMenu");
                });
        builder.with(InstanceSwitcherItemProperties.MORE_MENU, () -> listMenu);
        builder.with(
                InstanceSwitcherItemProperties.MORE_MENU_CONTENT_DESCRIPTION,
                mContext.getString(
                        R.string.instance_switcher_item_more_menu_content_description,
                        UiUtils.getItemTitle(mContext, item)));
    }

    private void closeWindow(InstanceInfo item) {
        if (canSkipConfirm(item)) {
            removeInstances(Collections.singletonList(item.instanceId));
        } else {
            showConfirmationMessage(item);
        }
    }

    private void selectInstance(InstanceInfo clickedItem) {
        int instanceId = clickedItem.instanceId;
        boolean wasSelected = mSelectedItems.contains(instanceId);

        if (UiUtils.isRobustWindowManagementBulkCloseEnabled()) {
            // Multi-selection is allowed. Toggle the clicked item.
            if (wasSelected) {
                mSelectedItems.remove(instanceId);
            } else {
                mSelectedItems.add(instanceId);
            }
        } else {
            // Single-selection. Clear everything, then select if it wasn't selected.
            mSelectedItems.clear();
            if (!wasSelected) {
                mSelectedItems.add(instanceId);
            }
        }

        // Update the UI models to reflect the new selection state.
        for (ListItem li : getCurrentList()) {
            int id = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
            li.model.set(InstanceSwitcherItemProperties.IS_SELECTED, mSelectedItems.contains(id));
        }

        updateActionButtons();
    }

    private void selectAllItems() {
        for (ListItem li : getCurrentList()) {
            int instanceId = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
            mSelectedItems.add(instanceId);
            li.model.set(InstanceSwitcherItemProperties.IS_SELECTED, true);
        }
        updateActionButtons();
    }

    private void updateActionButtons() {
        assumeNonNull(mDialog);
        int selectionCount = mSelectedItems.size();

        // 1. Update positive button state.
        boolean positiveButtonDisabled = true;
        if (selectionCount > 0) {
            if (UiUtils.isRobustWindowManagementBulkCloseEnabled()) {
                if (selectionCount == 1 && mActiveModelList.size() < mMaxInstanceCount) {
                    positiveButtonDisabled = false;
                }
            } else {
                if (!mIsInactiveListShowing || mActiveModelList.size() < mMaxInstanceCount) {
                    positiveButtonDisabled = false;
                }
            }
        }
        mDialog.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, positiveButtonDisabled);

        // Return early if Robust Window Management is not enabled.
        if (!UiUtils.isRobustWindowManagementBulkCloseEnabled()) return;
        // 2. Update title and back button visibility.
        mDialog.set(ModalDialogProperties.TITLE_BACK_BUTTON_VISIBLE, selectionCount > 0);
        String title =
                selectionCount > 0
                        ? mContext.getResources()
                                .getQuantityString(
                                        R.plurals.instance_switcher_windows_selected_header,
                                        selectionCount,
                                        String.valueOf(selectionCount))
                        : mContext.getString(R.string.instance_switcher_header);
        mDialog.set(ModalDialogProperties.TITLE, title);
        // 3. Update per-item buttons.
        boolean itemButtonsEnabled = selectionCount <= 1;
        for (ListItem li : getCurrentList()) {
            if (mIsInactiveListShowing) {
                li.model.set(
                        InstanceSwitcherItemProperties.CLOSE_BUTTON_ENABLED, itemButtonsEnabled);
            } else {
                li.model.set(InstanceSwitcherItemProperties.MORE_MENU_ENABLED, itemButtonsEnabled);
            }
        }
    }

    private void updatePositiveButtonText() {
        @StringRes int buttonLabelResId = mIsInactiveListShowing ? R.string.restore : R.string.open;
        assumeNonNull(mDialog);
        mDialog.set(
                ModalDialogProperties.POSITIVE_BUTTON_TEXT, mContext.getString(buttonLabelResId));
    }

    private void unselectItems(boolean hideVisibleList) {
        Iterator<ListItem> it =
                (hideVisibleList == mIsInactiveListShowing)
                        ? mInactiveModelList.iterator()
                        : mActiveModelList.iterator();
        while (it.hasNext()) {
            ListItem li = it.next();
            if (li.model.get(InstanceSwitcherItemProperties.IS_SELECTED)) {
                li.model.set(InstanceSwitcherItemProperties.IS_SELECTED, false);
            }
        }
        mSelectedItems.clear();
        updateActionButtons();
    }

    void dismissDialog(@DialogDismissalCause int cause) {
        mModalDialogManager.dismissDialog(mDialog, cause);
    }

    /**
     * Updates the command UI state when the dialog starts showing or needs refresh. Conditionally
     * show the "+New window" layout or the max_instance_info TextView depending on the instance
     * count.
     */
    private void updateCommandUiState(boolean isNewWindowEnabled) {
        assumeNonNull(mActiveListItemDecoration);
        assumeNonNull(mActiveInstancesList);
        assumeNonNull(mNewWindowLayout);
        mActiveListItemDecoration.setCommandUiEnabled(isNewWindowEnabled);
        mActiveInstancesList.invalidateItemDecorations();
        mNewWindowEnabled = isNewWindowEnabled;
        if (mNewWindowEnabled && !mIsInactiveListShowing) {
            mNewWindowLayout.setVisibility(View.VISIBLE);
            mNewWindowLayout.setOnClickListener(
                    view -> {
                        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
                        mDelegate.openNewWindow(mIsIncognitoWindow);
                    });
        } else {
            mNewWindowLayout.setVisibility(View.GONE);
        }
        updateMaxInfoTextView();
    }

    private void updateMaxInfoTextView() {
        assumeNonNull(mMaxInfoView);
        if (mNewWindowEnabled) {
            mMaxInfoView.setVisibility(View.GONE);
        } else {
            String text = mContext.getString(getMaxInfoTextRes(), mMaxInstanceCount - 1);
            mMaxInfoView.setText(text);
            mMaxInfoView.setVisibility(View.VISIBLE);
        }
    }

    private @StringRes int getMaxInfoTextRes() {
        if (UiUtils.isRobustWindowManagementEnabled()) {
            return mIsInactiveListShowing
                    ? R.string.max_number_of_windows_instance_switcher_inactive_tab
                    : R.string.max_number_of_windows_instance_switcher_active_tab;
        }
        return mIsInactiveListShowing
                ? R.string.max_number_of_windows_instance_switcher_v2_inactive_tab
                : R.string.max_number_of_windows_instance_switcher_v2_active_tab;
    }

    private boolean shouldAllowNewWindowCreation() {
        int instanceCount = mActiveModelList.size();
        if (!UiUtils.isRobustWindowManagementEnabled()) {
            instanceCount += mInactiveModelList.size();
        }
        return instanceCount < mMaxInstanceCount;
    }

    private void removeInstances(List<Integer> instanceIds) {
        for (Integer instanceId : instanceIds) {
            addLayoutListeners(
                    assumeNonNull(mDialogView),
                    assumeNonNull(mTabHeaderRow),
                    assumeNonNull(mInstanceListContainer),
                    assumeNonNull(mActiveInstancesList),
                    assumeNonNull(mInactiveInstancesList),
                    () -> mIsInactiveListShowing,
                    assumeNonNull(mNewWindowLayout),
                    mMinCommandItemHeightPx,
                    mItemPaddingHeightPx,
                    /* registerResizeListener= */ false);
            assert mDialog != null;
            mSelectedItems.remove(instanceId);
            updateActionButtons();
            removeItemFromModelList(
                    instanceId, mIsInactiveListShowing ? mInactiveModelList : mActiveModelList);
            updateActionButtons();
            updateCommandUiState(shouldAllowNewWindowCreation());
            updateTabTitle(mActiveModelList.size(), mInactiveModelList.size());
            updateMoreMenu();
            updateInactiveListEmptyStateVisibility();
        }
        mDelegate.closeInstances(instanceIds);
        RecordUserAction.record("Android.WindowManager.CloseWindow");
    }

    private void removeItemFromModelList(int instanceId, ModelList list) {
        for (ListItem li : list) {
            int id = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
            if (id == instanceId) {
                list.remove(li);
                return;
            }
        }
    }

    private static boolean canSkipConfirm(InstanceInfo item) {
        // Unrestorable, invisible instance can be deleted without confirmation.
        if (UiUtils.totalTabCount(item) == 0 && item.type == InstanceInfo.Type.OTHER) return true;
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, false);
    }

    @VisibleForTesting
    static void setSkipCloseConfirmation() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, true);
    }

    private void showConfirmationMessage(InstanceInfo item) {
        mItemToDelete = item;
        int style = R.style.Theme_Chromium_Multiwindow_CloseConfirmDialog;
        Dialog dialog = new Dialog(mContext, style);
        dialog.setCanceledOnTouchOutside(false);
        dialog.setContentView(R.layout.close_confirmation_dialog);
        Resources res = mContext.getResources();
        String title = res.getString(R.string.instance_switcher_close_confirm_header);
        if (item.type == InstanceInfo.Type.CURRENT) {
            title = res.getString(R.string.instance_switcher_close_confirm_header_current);
        }
        ((TextView) dialog.findViewById(R.id.title)).setText(title);
        TextView messageView = dialog.findViewById(R.id.message);
        messageView.setText(mUiUtils.getConfirmationMessage(item));

        TextView positiveButton = dialog.findViewById(R.id.positive_button);
        positiveButton.setText(res.getString(R.string.close));
        positiveButton.setOnClickListener(
                v -> {
                    assert mItemToDelete != null;
                    CheckBox skipConfirm = dialog.findViewById(R.id.no_more_check);
                    if (skipConfirm.isChecked()) setSkipCloseConfirmation();
                    dialog.dismiss();
                    removeInstances(Collections.singletonList(mItemToDelete.instanceId));
                });
        TextView negativeButton = dialog.findViewById(R.id.negative_button);
        negativeButton.setText(res.getString(R.string.cancel));
        negativeButton.setOnClickListener(
                v -> {
                    dialog.dismiss();
                });
        dialog.show();
    }

    private ModelList getCurrentList() {
        return mIsInactiveListShowing ? mInactiveModelList : mActiveModelList;
    }

    private @Nullable ListItem getInstanceListItem(InstanceInfo item) {
        for (ListItem listItem : getCurrentList()) {
            if (listItem.model.get(InstanceSwitcherItemProperties.INSTANCE_ID) == item.instanceId) {
                return listItem;
            }
        }
        return null;
    }

    private void showNameWindowDialog(InstanceInfo item) {
        ListItem listItem = assumeNonNull(getInstanceListItem(item));
        String currentTitle = listItem.model.get(InstanceSwitcherItemProperties.TITLE);

        Callback<String> nameChangedCallback =
                newTitle -> {
                    String customTitle = newTitle;
                    if (TextUtils.isEmpty(customTitle)) {
                        // Create a clone of the original item with an updated empty custom title to
                        // subsequently determine the default title when the custom title is
                        // cleared.
                        InstanceInfo updatedItem =
                                new InstanceInfo(
                                        item.instanceId,
                                        item.taskId,
                                        item.type,
                                        item.url,
                                        item.title,
                                        customTitle,
                                        item.tabCount,
                                        item.incognitoTabCount,
                                        item.isIncognitoSelected,
                                        item.lastAccessedTime,
                                        item.closureTime);
                        newTitle = UiUtils.getItemTitle(mContext, updatedItem);
                    }

                    listItem.model.set(InstanceSwitcherItemProperties.TITLE, newTitle);
                    listItem.model.set(
                            InstanceSwitcherItemProperties.MORE_MENU_CONTENT_DESCRIPTION,
                            mContext.getString(
                                    R.string.instance_switcher_item_more_menu_content_description,
                                    newTitle));
                    mDelegate.renameInstance(item.instanceId, customTitle);
                };

        UiUtils.showNameWindowDialog(
                mContext, currentTitle, nameChangedCallback, NameWindowDialogSource.WINDOW_MANAGER);
    }

    private void updateTabTitle(int numActiveInstances, int numInactiveInstances) {
        if (mTabHeaderRow == null) return;
        Tab activeTab = mTabHeaderRow.getTabAt(0);
        Tab inactiveTab = mTabHeaderRow.getTabAt(1);
        assumeNonNull(activeTab);
        assumeNonNull(inactiveTab);
        activeTab.setText(
                mContext.getString(R.string.instance_switcher_tabs_active, numActiveInstances));
        inactiveTab.setText(
                mContext.getString(R.string.instance_switcher_tabs_inactive, numInactiveInstances));
    }

    private void updateMoreMenu() {
        if (!UiUtils.isRobustWindowManagementBulkCloseEnabled()) return;
        assumeNonNull(mDialog);

        // If there are no visible instances in the current list, hide the more menu.
        mDialog.set(ModalDialogProperties.TITLE_MORE_BUTTON_VISIBLE, !getCurrentList().isEmpty());
        if (getCurrentList().isEmpty()) {
            return;
        }

        ModelList menuItems = new ModelList();

        if (mSelectedItems.size() < getCurrentList().size()) {
            menuItems.add(buildSimpleMenuItem(R.string.instance_switcher_select_all));
        }
        if (!mSelectedItems.isEmpty()) {
            menuItems.add(buildSimpleMenuItem(R.string.instance_switcher_deselect_all));
        }
        if (mSelectedItems.size() >= 2) {
            menuItems.add(buildSimpleMenuItem(R.string.instance_switcher_close_windows));
        } else if (mSelectedItems.size() == 1) {
            menuItems.add(buildSimpleMenuItem(R.string.instance_switcher_close_window));
        }

        ListMenu.Delegate delegate =
                (model, view) -> {
                    int textId = model.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.instance_switcher_select_all) {
                        selectAllItems();
                        updateMoreMenu();
                    } else if (textId == R.string.instance_switcher_deselect_all) {
                        unselectItems(/* hideVisibleList= */ true);
                        updateMoreMenu();
                    } else if (textId == R.string.instance_switcher_close_window
                            || textId == R.string.instance_switcher_close_windows) {
                        removeInstances(new ArrayList<>(mSelectedItems));
                        unselectItems(/* hideVisibleList= */ true);
                        updateMoreMenu();
                    }
                };

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(mContext, menuItems, delegate);

        mDialog.set(ModalDialogProperties.TITLE_MORE_BUTTON_DELEGATE, () -> listMenu);
    }

    private void updateInactiveListEmptyStateVisibility() {
        if (mInactiveListEmptyStateView == null) return;
        boolean showEmptyState = mIsInactiveListShowing && mInactiveModelList.isEmpty();
        mInactiveListEmptyStateView.setVisibility(showEmptyState ? View.VISIBLE : View.GONE);
    }

    private void buildWindowManagerMoreMenu(PropertyModel.Builder builder) {
        ModelList menuItems = new ModelList();
        menuItems.add(buildSimpleMenuItem(R.string.instance_switcher_select_all));
        ListMenu.Delegate delegate =
                (model, view) -> {
                    int textId = model.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.instance_switcher_select_all) {
                        selectAllItems();
                    }
                };
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(mContext, menuItems, delegate);
        builder.with(ModalDialogProperties.TITLE_MORE_BUTTON_DELEGATE, () -> listMenu);
    }
}
