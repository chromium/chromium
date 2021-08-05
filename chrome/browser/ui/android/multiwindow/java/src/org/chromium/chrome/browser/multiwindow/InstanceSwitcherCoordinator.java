// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Iterator;
import java.util.List;

/**
 * Coordinator to construct the instance switcher dialog.
 */
public class InstanceSwitcherCoordinator {
    /**
     * Type of the entries shown on the dialog.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({EntryType.INSTANCE, EntryType.COMMAND})
    private @interface EntryType {
        int INSTANCE = 0; // Instance item
        int COMMAND = 1; // Command "+New Window"
    }

    private final Context mContext;
    private final Callback<InstanceInfo> mOpenCallback;
    private final Callback<InstanceInfo> mCloseCallback;
    private final ModalDialogManager mModalDialogManager;

    private final ModelList mModelList = new ModelList();
    private final UiUtils mUiUtils;
    private final View mDialogView;

    private PropertyModel mDialog;

    /**
     * Show instance switcher modal dialog UI.
     * @param context Context to use to build the dialog.
     * @param modalDialogManager {@link ModalDialogManager} object.
     * @param openCallback Callback to invoke to open a chosen instance.
     * @param closeCallback Callback to invoke to close a chosen instance.
     * @param instanceInfo List of {@link InstanceInfo} for available Chrome instances.
     */
    public static void showDialog(Context context, ModalDialogManager modalDialogManager,
            Callback<InstanceInfo> openCallback, Callback<InstanceInfo> closeCallback,
            List<InstanceInfo> instanceInfo) {
        new InstanceSwitcherCoordinator(context, modalDialogManager, openCallback, closeCallback)
                .showDialog(instanceInfo);
    }

    private InstanceSwitcherCoordinator(Context context, ModalDialogManager modalDialogManager,
            Callback<InstanceInfo> openCallback, Callback<InstanceInfo> closeCallback) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOpenCallback = openCallback;
        mCloseCallback = closeCallback;
        mUiUtils = new UiUtils(mContext);

        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        // TODO: Extend modern_list_item_view.xml to replace instance_switcher_item.xml
        adapter.registerType(EntryType.INSTANCE,
                parentView
                -> LayoutInflater.from(mContext).inflate(R.layout.instance_switcher_item, null),
                InstanceSwitcherItemViewBinder::bind);
        mDialogView = LayoutInflater.from(context).inflate(R.layout.instance_switcher_dialog, null);
        ListView listView = (ListView) mDialogView.findViewById(R.id.list_view);
        listView.setAdapter(adapter);
    }

    private void showDialog(List<InstanceInfo> items) {
        for (int i = 0; i < items.size(); ++i) {
            PropertyModel itemModel = generateListItem(items.get(i));
            mModelList.add(new ModelListAdapter.ListItem(EntryType.INSTANCE, itemModel));
        }
        // TODO: Add "+ New Window" menu item at the bottom of the list.
        mDialog = createDialog(mDialogView, mModelList, items);
        mModalDialogManager.showDialog(mDialog, ModalDialogType.APP);
    }

    private PropertyModel createDialog(
            View dialogView, ModelList modelList, List<InstanceInfo> items) {
        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {}

            @Override
            public void onClick(PropertyModel model, int buttonType) {
                switch (buttonType) {
                    case ModalDialogProperties.ButtonType.POSITIVE:
                        dismissDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        break;
                    default:
                }
            }
        };
        Resources resources = mContext.getResources();
        String title = mContext.getString(R.string.instance_switcher_header);
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                .with(ModalDialogProperties.TITLE, title)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.cancel)
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
                        .with(InstanceSwitcherItemProperties.CLICK_LISTENER,
                                (view) -> switchToInstance(item));
        if (!currentIndicator) buildMoreMenu(builder, item);
        PropertyModel model = builder.build();
        mUiUtils.setFavicon(model, InstanceSwitcherItemProperties.FAVICON, item);
        return model;
    }

    private void buildMoreMenu(PropertyModel.Builder builder, InstanceInfo item) {
        ModelList moreMenu = new ModelList();
        moreMenu.add(buildMenuListItem(R.string.instance_switcher_close_window, 0, 0));
        ListMenu.Delegate moreMenuDelegate = (model) -> {
            int textId = model.get(ListMenuItemProperties.TITLE_ID);
            if (textId == R.string.instance_switcher_close_window) {
                if (item.tabCount == 0 && item.type == InstanceInfo.Type.OTHER) {
                    removeInstance(item);
                } else {
                    // TODO: Show confirmation dialog instead.
                    removeInstance(item);
                }
            }
        };
        builder.with(InstanceSwitcherItemProperties.MORE_MENU,
                () -> new BasicListMenu(mContext, moreMenu, moreMenuDelegate));
    }

    private void switchToInstance(InstanceInfo item) {
        if (item.type == InstanceInfo.Type.CURRENT || item.type == InstanceInfo.Type.ADJACENT) {
            Toast.makeText(mContext, R.string.instance_switcher_already_running_foreground,
                         Toast.LENGTH_LONG)
                    .show();
            return;
        }
        dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
        mOpenCallback.onResult(item);
    }

    private void dismissDialog(@DialogDismissalCause int cause) {
        mModalDialogManager.dismissDialog(mDialog, cause);
    }

    private void removeInstance(InstanceInfo item) {
        int instanceId = item.instanceId;
        Iterator<ListItem> it = mModelList.iterator();
        while (it.hasNext()) {
            ListItem li = it.next();
            int id = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
            if (id == instanceId) {
                mModelList.remove(li);
                break;
            }
        }
        mCloseCallback.onResult(item);
    }
}
