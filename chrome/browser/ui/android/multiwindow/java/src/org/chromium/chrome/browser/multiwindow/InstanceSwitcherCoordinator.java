// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.UiUtils.INVALID_TASK_ID;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.IntDef;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Coordinator to construct the instance switcher dialog.
 * TODO: Resolve various inconsistencies that can be caused by Ui from multiple instances.
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
    private final Runnable mNewWindowAction;
    private final ModalDialogManager mModalDialogManager;

    private final ModelList mModelList = new ModelList();
    private final ModelList mActiveModelList = new ModelList();
    private final ModelList mInactiveModelList = new ModelList();
    private final UiUtils mUiUtils;
    private final View mDialogView;
    private @Nullable TabLayout mTabHeaderRow;

    private @Nullable PropertyModel mDialog;
    private @Nullable InstanceInfo mItemToDelete;
    private @Nullable PropertyModel mNewWindowModel;
    private boolean mNewWindowEnabled;

    /**
     * Show instance switcher modal dialog UI.
     * @param context Context to use to build the dialog.
     * @param modalDialogManager {@link ModalDialogManager} object.
     * @param iconBridge An object that fetches favicons from local DB.
     * @param openCallback Callback to invoke to open a chosen instance.
     * @param closeCallback Callback to invoke to close a chosen instance.
     * @param newWindowAction Runnable to invoke to open a new window.
     * @param newWindowEnabled True if the "New window" command needs to be enabled.
     * @param instanceInfo List of {@link InstanceInfo} for available Chrome instances.
     */
    public static void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            Callback<InstanceInfo> openCallback,
            Callback<InstanceInfo> closeCallback,
            Runnable newWindowAction,
            boolean newWindowEnabled,
            List<InstanceInfo> instanceInfo) {
        new InstanceSwitcherCoordinator(
                        context,
                        modalDialogManager,
                        iconBridge,
                        openCallback,
                        closeCallback,
                        newWindowAction)
                .show(instanceInfo, newWindowEnabled);
    }

    private InstanceSwitcherCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            LargeIconBridge iconBridge,
            Callback<InstanceInfo> openCallback,
            Callback<InstanceInfo> closeCallback,
            Runnable newWindowAction) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOpenCallback = openCallback;
        mCloseCallback = closeCallback;
        mUiUtils = new UiUtils(mContext, iconBridge);
        mNewWindowAction = newWindowAction;

        if (isInstanceSwitcherV2Enabled()) {
            var activeListAdapter = getInstanceListAdapter(/* active= */ true);
            var inactiveListAdapter = getInstanceListAdapter(/* active= */ false);

            mDialogView =
                    LayoutInflater.from(context)
                            .inflate(R.layout.instance_switcher_dialog_v2, null);

            int itemVerticalSpacing =
                    mContext.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.instance_switcher_dialog_list_item_padding);
            var itemDecoration = new DialogListItemDecoration(itemVerticalSpacing);

            RecyclerView activeInstancesList = mDialogView.findViewById(R.id.active_instance_list);
            activeInstancesList.setLayoutManager(
                    new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
            activeInstancesList.setAdapter(activeListAdapter);
            activeInstancesList.addItemDecoration(itemDecoration);

            RecyclerView inactiveInstancesList =
                    mDialogView.findViewById(R.id.inactive_instance_list);
            inactiveInstancesList.setLayoutManager(
                    new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
            inactiveInstancesList.setAdapter(inactiveListAdapter);
            inactiveInstancesList.addItemDecoration(itemDecoration);

            mTabHeaderRow = mDialogView.findViewById(R.id.tabs);
            mTabHeaderRow.addOnTabSelectedListener(
                    new OnTabSelectedListener() {
                        @Override
                        public void onTabSelected(Tab tab) {
                            boolean isActiveTab = tab.getPosition() == 0;
                            activeInstancesList.setVisibility(
                                    isActiveTab ? View.VISIBLE : View.GONE);
                            inactiveInstancesList.setVisibility(
                                    isActiveTab ? View.GONE : View.VISIBLE);
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

    private SimpleRecyclerViewAdapter getInstanceListAdapter(boolean active) {
        var adapter = new SimpleRecyclerViewAdapter(active ? mActiveModelList : mInactiveModelList);
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
        return adapter;
    }

    private void show(List<InstanceInfo> items, boolean newWindowEnabled) {
        UiUtils.closeOpenDialogs();
        sPrevInstance = this;
        for (int i = 0; i < items.size(); ++i) {
            // An active instance should have an associated live task.
            boolean isActiveInstance = items.get(i).taskId != INVALID_TASK_ID;
            PropertyModel itemModel = generateListItem(items.get(i));
            if (isInstanceSwitcherV2Enabled()) {
                if (isActiveInstance) {
                    mActiveModelList.add(
                            new ModelListAdapter.ListItem(EntryType.INSTANCE, itemModel));
                } else {
                    mInactiveModelList.add(
                            new ModelListAdapter.ListItem(EntryType.INSTANCE, itemModel));
                }
            } else {
                mModelList.add(new ModelListAdapter.ListItem(EntryType.INSTANCE, itemModel));
            }
        }
        mNewWindowModel = new PropertyModel(InstanceSwitcherItemProperties.ALL_KEYS);
        enableNewWindowCommand(newWindowEnabled);
        mModelList.add(new ModelListAdapter.ListItem(EntryType.COMMAND, mNewWindowModel));
        if (isInstanceSwitcherV2Enabled()) {
            // "+New window" should only be added to the active instances list.
            mActiveModelList.add(new ModelListAdapter.ListItem(EntryType.COMMAND, mNewWindowModel));
            // Exclude COMMAND item from active instance count.
            int numActiveInstances = mActiveModelList.size() - 1;
            updateTabTitle(numActiveInstances, mInactiveModelList.size());
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
                        }
                    }
                };
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.instance_switcher_header);
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                .with(ModalDialogProperties.TITLE, title)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, null)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources, R.string.cancel)
                .with(
                        ModalDialogProperties.DIALOG_STYLES,
                        ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                .build();
    }

    private PropertyModel generateListItem(InstanceInfo item) {
        String title = mUiUtils.getItemTitle(item);
        String desc = mUiUtils.getItemDesc(item);
        boolean currentIndicator = item.type == InstanceInfo.Type.CURRENT;
        PropertyModel.Builder builder =
                new PropertyModel.Builder(InstanceSwitcherItemProperties.ALL_KEYS)
                        .with(InstanceSwitcherItemProperties.TITLE, title)
                        .with(InstanceSwitcherItemProperties.DESC, desc)
                        .with(InstanceSwitcherItemProperties.CURRENT, currentIndicator)
                        .with(InstanceSwitcherItemProperties.INSTANCE_ID, item.instanceId)
                        .with(
                                InstanceSwitcherItemProperties.CLICK_LISTENER,
                                (view) -> switchToInstance(item));
        if (!currentIndicator) buildMoreMenu(builder, item);
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
        }
        mNewWindowEnabled = enabled;
    }

    private void newWindowAction(View view) {
        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        mNewWindowAction.run();
    }

    private void buildMoreMenu(PropertyModel.Builder builder, InstanceInfo item) {
        ModelList moreMenu = new ModelList();
        moreMenu.add(buildMenuListItem(R.string.instance_switcher_close_window, 0, 0));
        ListMenu.Delegate moreMenuDelegate =
                (model) -> {
                    int textId = model.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.instance_switcher_close_window) {
                        if (canSkipConfirm(item)) {
                            removeInstance(item);
                        } else {
                            showConfirmationMessage(item);
                        }
                    }
                };
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(mContext, moreMenu, moreMenuDelegate);
        listMenu.addContentViewClickRunnable(
                () -> {
                    RecordUserAction.record("Android.WindowManager.SecondaryMenu");
                });
        builder.with(InstanceSwitcherItemProperties.MORE_MENU, () -> listMenu);
    }

    private void switchToInstance(InstanceInfo item) {
        if (item.type == InstanceInfo.Type.CURRENT || item.type == InstanceInfo.Type.ADJACENT) {
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

    void dismissDialog(@DialogDismissalCause int cause) {
        mModalDialogManager.dismissDialog(mDialog, cause);
    }

    private void removeInstance(InstanceInfo item) {
        int instanceId = item.instanceId;

        if (isInstanceSwitcherV2Enabled()) {
            removeItemFromModelList(
                    instanceId,
                    item.taskId == INVALID_TASK_ID ? mInactiveModelList : mActiveModelList);
        } else {
            removeItemFromModelList(instanceId, mModelList);
        }

        mCloseCallback.onResult(item);
        RecordUserAction.record("Android.WindowManager.CloseWindow");
        // Removing an instance enables the new window item.
        enableNewWindowCommand(true);

        if (isInstanceSwitcherV2Enabled()) {
            // Exclude COMMAND item from active instance count.
            int numActiveInstances = mActiveModelList.size() - 1;
            int numInactiveInstances = mInactiveModelList.size();
            updateTabTitle(numActiveInstances, numInactiveInstances);
        }
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
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
                    dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                });
        dialog.show();
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

    private static boolean isInstanceSwitcherV2Enabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INSTANCE_SWITCHER_V2);
    }
}
