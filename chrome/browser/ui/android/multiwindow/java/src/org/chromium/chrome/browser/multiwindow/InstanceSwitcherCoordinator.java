// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.UiUtils.INVALID_TASK_ID;
import static org.chromium.components.browser_ui.widget.ListItemBuilder.buildSimpleMenuItem;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.util.TimeTextResolver;
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
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

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

    /** Type of the entries shown on the dialog. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({EntryType.INSTANCE, EntryType.COMMAND})
    private @interface EntryType {
        int INSTANCE = 0; // Instance item
        int COMMAND = 1; // Command "+New Window"
    }

    private final Context mContext;
    private final Callback<InstanceInfo> mOpenCallback;
    private final Callback<InstanceInfo> mCloseCallback;
    private final Callback<Pair<Integer, String>> mRenameWindowCallback;
    private final Runnable mNewWindowAction;
    private final ModalDialogManager mModalDialogManager;
    private final int mMaxInstanceCount;

    private final ModelList mModelList = new ModelList();
    private final ModelList mActiveModelList = new ModelList();
    private final ModelList mInactiveModelList = new ModelList();
    private final UiUtils mUiUtils;
    private final View mDialogView;
    private final boolean mIsIncognitoWindow;
    private @Nullable TabLayout mTabHeaderRow;

    private @Nullable PropertyModel mDialog;
    private @Nullable InstanceInfo mItemToDelete;
    private @Nullable PropertyModel mNewWindowModel;
    private @MonotonicNonNull LinearLayout mNewWindowLayout;
    private @MonotonicNonNull TextView mMaxInfoView;
    private final HashMap<Integer, InstanceInfo> mSelectedItems;
    private boolean mNewWindowEnabled;
    private boolean mIsInactiveListShowing;
    private @MonotonicNonNull FrameLayout mInstanceListContainer;
    private @MonotonicNonNull RecyclerView mActiveInstancesList;
    private @MonotonicNonNull RecyclerView mInactiveInstancesList;
    private @MonotonicNonNull DialogListItemDecoration mActiveListItemDecoration;

    /**
     * Show instance switcher modal dialog UI.
     *
     * @param context Context to use to build the dialog.
     * @param modalDialogManager {@link ModalDialogManager} object.
     * @param iconBridge An object that fetches favicons from local DB.
     * @param openCallback Callback to invoke to open a chosen instance.
     * @param closeCallback Callback to invoke to close a chosen instance.
     * @param renameWindowCallback Callback to invoke to rename a chosen instance.
     * @param newWindowAction Runnable to invoke to open a new window.
     * @param maxInstanceCount The maximum number of instances whose state can be persisted.
     * @param instanceInfo List of {@link InstanceInfo} for available Chrome instances.
     * @param isIncognitoWindow Used to determine if dialog should show "New window" or "New
     *     Incognito window".
     */
    public static void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            Callback<InstanceInfo> openCallback,
            Callback<InstanceInfo> closeCallback,
            Callback<Pair<Integer, String>> renameWindowCallback,
            Runnable newWindowAction,
            int maxInstanceCount,
            List<InstanceInfo> instanceInfo,
            boolean isIncognitoWindow) {
        new InstanceSwitcherCoordinator(
                        context,
                        modalDialogManager,
                        iconBridge,
                        openCallback,
                        closeCallback,
                        renameWindowCallback,
                        newWindowAction,
                        maxInstanceCount,
                        isIncognitoWindow)
                .show(instanceInfo);
    }

    private InstanceSwitcherCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            Callback<InstanceInfo> openCallback,
            Callback<InstanceInfo> closeCallback,
            Callback<Pair<Integer, String>> renameWindowCallback,
            Runnable newWindowAction,
            int maxInstanceCount,
            boolean isIncognitoWindow) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOpenCallback = openCallback;
        mCloseCallback = closeCallback;
        mRenameWindowCallback = renameWindowCallback;
        mUiUtils = new UiUtils(mContext, iconBridge);
        mNewWindowAction = newWindowAction;
        mMaxInstanceCount = maxInstanceCount;
        mIsIncognitoWindow = isIncognitoWindow;
        mSelectedItems = new HashMap<>();

        if (UiUtils.isInstanceSwitcherV2Enabled()) {
            var activeListAdapter = getInstanceListV2Adapter(/* active= */ true);
            var inactiveListAdapter = getInstanceListV2Adapter(/* active= */ false);

            mDialogView =
                    LayoutInflater.from(context)
                            .inflate(R.layout.instance_switcher_dialog_v2, null);
            mInstanceListContainer = mDialogView.findViewById(R.id.instance_list_container);
            mMaxInfoView = mDialogView.findViewById(R.id.max_instance_info);
            mNewWindowLayout = mDialogView.findViewById(R.id.new_window);
            TextView newWindowTextView = mNewWindowLayout.findViewById(R.id.new_window_text);
            if (mIsIncognitoWindow) {
                newWindowTextView.setText(R.string.menu_new_incognito_window);
            }

            int itemVerticalSpacing =
                    mContext.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.instance_switcher_dialog_list_item_padding);
            mActiveListItemDecoration = new DialogListItemDecoration(itemVerticalSpacing);
            var inactiveListItemDecoration = new DialogListItemDecoration(itemVerticalSpacing);

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

            addInstanceListGlobalLayoutListener(
                    mInstanceListContainer, mActiveInstancesList, mIsInactiveListShowing);

            mTabHeaderRow = mDialogView.findViewById(R.id.tabs);
            mTabHeaderRow.addOnTabSelectedListener(
                    new OnTabSelectedListener() {
                        @Override
                        public void onTabSelected(Tab tab) {
                            boolean isActiveTab = tab.getPosition() == 0;
                            mActiveInstancesList.setVisibility(
                                    isActiveTab ? View.VISIBLE : View.GONE);
                            mInactiveInstancesList.setVisibility(
                                    isActiveTab ? View.GONE : View.VISIBLE);
                            mIsInactiveListShowing = !isActiveTab;
                            addInstanceListGlobalLayoutListener(
                                    mInstanceListContainer,
                                    mActiveInstancesList,
                                    mIsInactiveListShowing);
                            updateCommandUiState(getTotalInstanceCount() < mMaxInstanceCount);
                            unselectItems();
                            updatePositiveButtonText();
                        }

                        @Override
                        public void onTabUnselected(Tab tab) {}

                        @Override
                        public void onTabReselected(Tab tab) {}
                    });
        } else {
            ModelListAdapter adapter = new ModelListAdapter(mModelList);
            // TODO: Extend modern_list_item_view.xml to replace instance_switcher_item.xml
            adapter.registerType(
                    EntryType.INSTANCE,
                    parentView ->
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.instance_switcher_item, null),
                    InstanceSwitcherItemViewBinder::bind);
            adapter.registerType(
                    EntryType.COMMAND,
                    parentView ->
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.instance_switcher_cmd_item, null),
                    InstanceSwitcherItemViewBinder::bind);

            mDialogView =
                    LayoutInflater.from(context).inflate(R.layout.instance_switcher_dialog, null);
            ListView listView = mDialogView.findViewById(R.id.list_view);
            listView.setAdapter(adapter);
        }
    }

    private SimpleRecyclerViewAdapter getInstanceListV2Adapter(boolean active) {
        var adapter = new SimpleRecyclerViewAdapter(active ? mActiveModelList : mInactiveModelList);
        adapter.registerType(
                EntryType.INSTANCE,
                parentView ->
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.instance_switcher_item_v2, null),
                InstanceSwitcherItemViewBinder::bind);
        return adapter;
    }

    // Adds a listener to layout the command item correctly relative to the instance list view.
    /* package */ static OnGlobalLayoutListener addInstanceListGlobalLayoutListener(
            View instanceListContainer,
            RecyclerView activeInstancesList,
            boolean isInactiveListShowing) {
        var listener =
                new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        instanceListContainer
                                .getViewTreeObserver()
                                .removeOnGlobalLayoutListener(this);
                        maybeUpdateInstanceListContainerParams(
                                instanceListContainer, activeInstancesList, isInactiveListShowing);
                    }
                };
        instanceListContainer.getViewTreeObserver().addOnGlobalLayoutListener(listener);
        return listener;
    }

    private static void maybeUpdateInstanceListContainerParams(
            View instanceListContainer,
            RecyclerView activeInstancesList,
            boolean isInactiveListShowing) {
        LayoutParams params = (LayoutParams) instanceListContainer.getLayoutParams();

        // Default height / weight params should be applied for the inactive instance list, or a
        // scrollable active instance list so that the command item sticks while the instance list
        // is scrolled.
        boolean shouldUseDefaultWeight =
                isInactiveListShowing
                        || activeInstancesList.getMeasuredHeight()
                                < activeInstancesList.computeVerticalScrollRange();

        // Do nothing if params are already as expected.
        if ((shouldUseDefaultWeight && params.weight == 1f)
                || (!shouldUseDefaultWeight && params.weight == 0)) {
            return;
        }

        if (shouldUseDefaultWeight) {
            params.weight = 1f;
            params.height = 0;
        } else {
            // Special height / weight params, for when the active instance list does not have
            // enough items to make it scrollable. It is specifically required in a fullscreen
            // dialog layout where the list will occupy most of the container space pushing the
            // command item to the bottom of the container if weight=1, which is not desirable.
            params.weight = 0f;
            params.height = LayoutParams.WRAP_CONTENT;
        }
        instanceListContainer.setLayoutParams(params);
    }

    private void show(List<InstanceInfo> items) {
        UiUtils.closeOpenDialogs();
        sPrevInstance = this;
        for (int i = 0; i < items.size(); ++i) {
            // An active instance should have an associated live task.
            boolean isActiveInstance = items.get(i).taskId != INVALID_TASK_ID;
            PropertyModel itemModel = generateListItem(items.get(i));
            if (UiUtils.isInstanceSwitcherV2Enabled()) {
                if (isActiveInstance) {
                    mActiveModelList.add(new ListItem(EntryType.INSTANCE, itemModel));
                } else {
                    mInactiveModelList.add(new ListItem(EntryType.INSTANCE, itemModel));
                }
            } else {
                mModelList.add(new ListItem(EntryType.INSTANCE, itemModel));
            }
        }
        mNewWindowModel = new PropertyModel(InstanceSwitcherItemProperties.ALL_KEYS);

        if (UiUtils.isInstanceSwitcherV2Enabled()) {
            // Update UI state for instance switcher v2.
            updateCommandUiState(getTotalInstanceCount() < mMaxInstanceCount);
            updateTabTitle(mActiveModelList.size(), mInactiveModelList.size());
        } else {
            // Add new window command item to the list for v1.
            enableNewWindowCommand(items.size() < mMaxInstanceCount);
            mModelList.add(new ListItem(EntryType.COMMAND, mNewWindowModel));
        }

        mDialog = createDialog(mDialogView);
        mModalDialogManager.showDialog(mDialog, ModalDialogType.APP);
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
                                InstanceInfo selectedItem =
                                        mSelectedItems.entrySet().iterator().next().getValue();
                                String userAction =
                                        mIsInactiveListShowing
                                                ? "Android.WindowManager.OpenInactiveWindow"
                                                : "Android.WindowManager.OpenActiveWindow";
                                RecordUserAction.record(userAction);
                                switchToInstance(selectedItem);
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

        if (UiUtils.isInstanceSwitcherV2Enabled()) {
            builder.with(
                    ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                    resources,
                    mIsInactiveListShowing ? R.string.restore : R.string.open);
            builder.with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
        }
        return builder.build();
    }

    private PropertyModel generateListItem(InstanceInfo item) {
        String title = mUiUtils.getItemTitle(item);
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
                                    if (UiUtils.isInstanceSwitcherV2Enabled()) {
                                        selectInstance(item);
                                    } else {
                                        switchToInstance(item);
                                    }
                                });

        if (!UiUtils.isInstanceSwitcherV2Enabled()) {
            builder.with(InstanceSwitcherItemProperties.CURRENT, isCurrentWindow);
            if (!isCurrentWindow) {
                buildMoreMenu(builder, item);
            }
        } else {
            if (item.taskId != INVALID_TASK_ID) {
                buildMoreMenu(builder, item);
            } else {
                builder.with(
                        InstanceSwitcherItemProperties.CLOSE_BUTTON_CLICK_LISTENER,
                        (view) -> closeWindow(item));
                builder.with(InstanceSwitcherItemProperties.CLOSE_BUTTON_ENABLED, true);
                builder.with(
                        InstanceSwitcherItemProperties.CLOSE_BUTTON_CONTENT_DESCRIPTION,
                        mContext.getString(
                                R.string.instance_switcher_item_close_content_description,
                                mUiUtils.getItemTitle(item)));
            }
            String lastAccessedString =
                    isCurrentWindow
                            ? mContext.getString(R.string.instance_last_accessed_current)
                            : TimeTextResolver.resolveTimeAgoText(
                                    mContext.getResources(), item.lastAccessedTime);
            builder.with(InstanceSwitcherItemProperties.LAST_ACCESSED, lastAccessedString);
            builder.with(InstanceSwitcherItemProperties.IS_SELECTED, false);
        }
        PropertyModel model = builder.build();
        mUiUtils.setFavicon(model, InstanceSwitcherItemProperties.FAVICON, item);
        return model;
    }

    private void enableNewWindowCommand(boolean enabled) {
        if (mNewWindowEnabled && enabled) return;
        assumeNonNull(mNewWindowModel);
        mNewWindowModel.set(InstanceSwitcherItemProperties.ENABLE_COMMAND, enabled);
        if (enabled) {
            mNewWindowModel.set(
                    InstanceSwitcherItemProperties.CLICK_LISTENER, this::newWindowAction);
        } else {
            mNewWindowModel.set(
                    InstanceSwitcherItemProperties.MAX_INFO_TEXT,
                    mContext.getString(R.string.max_number_of_windows, mMaxInstanceCount));
        }
        mNewWindowEnabled = enabled;
    }

    private void newWindowAction(View view) {
        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        mNewWindowAction.run();
    }

    private void buildMoreMenu(PropertyModel.Builder builder, InstanceInfo item) {
        ModelList moreMenu = new ModelList();
        if (UiUtils.isInstanceSwitcherV2Enabled()) {
            if (UiUtils.isRobustWindowManagementEnabled()) {
                moreMenu.add(buildSimpleMenuItem(R.string.instance_switcher_name_window));
            }
            moreMenu.add(buildSimpleMenuItem(R.string.close));
        } else {
            moreMenu.add(buildSimpleMenuItem(R.string.instance_switcher_close_window));
        }

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
                        mUiUtils.getItemTitle(item)));
    }

    private void closeWindow(InstanceInfo item) {
        if (canSkipConfirm(item)) {
            removeInstance(item);
        } else {
            showConfirmationMessage(item);
        }
    }

    private void switchToInstance(InstanceInfo item) {
        if (!UiUtils.isInstanceSwitcherV2Enabled() && item.type == InstanceInfo.Type.CURRENT) {
            Toast.makeText(
                            mContext,
                            R.string.instance_switcher_already_running_foreground,
                            Toast.LENGTH_LONG)
                    .show();
            return;
        }
        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        mOpenCallback.onResult(item);
    }

    private void selectInstance(InstanceInfo clickedItem) {
        int instanceId = clickedItem.instanceId;
        boolean wasSelected = mSelectedItems.containsKey(instanceId);

        if (UiUtils.isRobustWindowManagementBulkCloseEnabled()) {
            // Multi-selection is allowed. Toggle the clicked item.
            if (wasSelected) {
                mSelectedItems.remove(instanceId);
            } else {
                mSelectedItems.put(instanceId, clickedItem);
            }
        } else {
            // Single-selection. Clear everything, then select if it wasn't selected.
            mSelectedItems.clear();
            if (!wasSelected) {
                mSelectedItems.put(instanceId, clickedItem);
            }
        }

        // Update the UI models to reflect the new selection state.
        for (ListItem li : getCurrentList()) {
            int id = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
            li.model.set(
                    InstanceSwitcherItemProperties.IS_SELECTED, mSelectedItems.containsKey(id));
        }

        updateWindowActionButtons();
    }

    private void updateWindowActionButtons() {
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

        // 2. Update per-item buttons (for robust mode).
        if (!UiUtils.isRobustWindowManagementBulkCloseEnabled()) return;
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
        if (!UiUtils.isInstanceSwitcherV2Enabled()) return;
        @StringRes int buttonLabelResId = mIsInactiveListShowing ? R.string.restore : R.string.open;
        assumeNonNull(mDialog);
        mDialog.set(
                ModalDialogProperties.POSITIVE_BUTTON_TEXT, mContext.getString(buttonLabelResId));
    }

    private void unselectItems() {
        // Unselect the items from the list that is being hidden.
        Iterator<ListItem> it =
                mIsInactiveListShowing
                        ? mActiveModelList.iterator()
                        : mInactiveModelList.iterator();
        while (it.hasNext()) {
            ListItem li = it.next();
            if (li.model.get(InstanceSwitcherItemProperties.IS_SELECTED)) {
                li.model.set(InstanceSwitcherItemProperties.IS_SELECTED, false);
            }
        }
        mSelectedItems.clear();
        updateWindowActionButtons();
    }

    void dismissDialog(@DialogDismissalCause int cause) {
        mModalDialogManager.dismissDialog(mDialog, cause);
    }

    /**
     * Updates the command UI state for Instance Switcher V2 when the dialog starts showing or needs
     * refresh. Conditionally show the "+New window" layout or the max_instance_info TextView
     * depending on the instance count.
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
            mNewWindowLayout.setOnClickListener(this::newWindowAction);
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

    private int getTotalInstanceCount() {
        if (!UiUtils.isInstanceSwitcherV2Enabled()) {
            // Exclude COMMAND item from list size.
            return mModelList.size() - 1;
        }
        int numActiveInstances = mActiveModelList.size();
        int numInactiveInstances = mInactiveModelList.size();
        return numActiveInstances + numInactiveInstances;
    }

    private void removeInstance(InstanceInfo item) {
        int instanceId = item.instanceId;
        if (UiUtils.isInstanceSwitcherV2Enabled()) {
            addInstanceListGlobalLayoutListener(
                    assumeNonNull(mInstanceListContainer),
                    assumeNonNull(mActiveInstancesList),
                    mIsInactiveListShowing);
            InstanceInfo selectedItem = mSelectedItems.get(instanceId);
            if (selectedItem != null && selectedItem.instanceId == item.instanceId) {
                assert mDialog != null;
                mDialog.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
            }
            removeItemFromModelList(
                    instanceId,
                    item.taskId == INVALID_TASK_ID ? mInactiveModelList : mActiveModelList);
            updateCommandUiState(getTotalInstanceCount() < mMaxInstanceCount);
            updateTabTitle(mActiveModelList.size(), mInactiveModelList.size());
        } else {
            removeItemFromModelList(instanceId, mModelList);
            // Update new window item based on instance count after instance removal.
            enableNewWindowCommand(getTotalInstanceCount() < mMaxInstanceCount);
        }
        mCloseCallback.onResult(item);
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
        if (UiUtils.isInstanceSwitcherV2Enabled() && item.type == InstanceInfo.Type.CURRENT) {
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
                    removeInstance(mItemToDelete);
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

    @Nullable
    private ListItem getInstanceListItem(InstanceInfo item) {
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
                        // Default to active tab title if custom title is cleared.
                        newTitle = item.title;
                    }

                    listItem.model.set(InstanceSwitcherItemProperties.TITLE, newTitle);
                    listItem.model.set(
                            InstanceSwitcherItemProperties.MORE_MENU_CONTENT_DESCRIPTION,
                            mContext.getString(
                                    R.string.instance_switcher_item_more_menu_content_description,
                                    newTitle));
                    mRenameWindowCallback.onResult(new Pair<>(item.instanceId, customTitle));
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
}
