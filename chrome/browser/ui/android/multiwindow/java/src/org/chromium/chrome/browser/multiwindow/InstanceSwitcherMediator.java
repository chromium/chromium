// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Iterator;
import java.util.List;

class InstanceSwitcherMediator {
    interface OpenAction {
        void run(int instanceId, int taskId, boolean openAdjacently);
    }

    interface CloseAction {
        void run(int instanceId, int taskId);
    }

    private final Context mContext;
    private final OpenAction mOpenAction;
    private final CloseAction mCloseAction;
    private final LargeIconBridge mLargeIconBridge;
    private final RoundedIconGenerator mIconGenerator;
    private final ModalDialogManager mModalDialogManager;
    private final int mMinIconSizeDp;
    private final int mDisplayedIconSize;
    private final ModelList mModelList;

    private PropertyModel mDialogModel;

    InstanceSwitcherMediator(
            Context context, ModelList modelList, OpenAction openAction, CloseAction closeAction) {
        mContext = context;
        mModelList = modelList;
        mOpenAction = openAction;
        mCloseAction = closeAction;

        Resources res = context.getResources();
        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        mMinIconSizeDp = (int) res.getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = res.getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(res);
        mModalDialogManager = new ModalDialogManager(
                new AppModalPresenter(mContext), ModalDialogManager.ModalDialogType.APP);
    }

    void showDialog(View dialogView, List<InstanceInfo> items) {
        // TODO: Remove dependency on View.
        mDialogModel = createDialog(dialogView, items);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private PropertyModel createDialog(View dialogView, List<InstanceInfo> items) {
        for (int i = 0; i < items.size(); ++i) {
            PropertyModel itemModel = generateListItem(items.get(i));
            mModelList.add(new ModelListAdapter.ListItem(0, itemModel));
        }
        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {}

            @Override
            public void onClick(PropertyModel model, int buttonType) {
                switch (buttonType) {
                    case ModalDialogProperties.ButtonType.POSITIVE:
                        mModalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
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
        GURL url = new GURL(item.url);

        // Use title for native pages to avoid showing internal URL like 'chrome-native://newtab'.
        // |incognito| can be ignored since we never pass recent tabs page in incognito mode.
        String title = NativePage.isNativePageUrl(url, /*incognito=*/false)
                ? item.title
                : UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(url);
        String desc;
        boolean currentIndicator = false;
        if (item.type == InstanceInfo.Type.CURRENT) {
            desc = mContext.getResources().getString(R.string.instance_switcher_current_window);
            currentIndicator = true;
        } else if (item.type == InstanceInfo.Type.ADJACENT) {
            desc = mContext.getResources().getString(R.string.instance_switcher_adjacent_window);
        } else {
            desc = mContext.getResources().getQuantityString(
                    R.plurals.instance_switcher_tab_count, item.tabCount, item.tabCount);
        }
        ModelList moreMenu = new ModelList();
        moreMenu.add(buildMenuListItem(R.string.instance_switcher_open_fullscreen, 0, 0));
        moreMenu.add(buildMenuListItem(R.string.instance_switcher_close_window, 0, 0));
        ListMenu.Delegate moreMenuDelegate = (model) -> {
            int textId = model.get(ListMenuItemProperties.TITLE_ID);
            if (textId == R.string.instance_switcher_open_fullscreen) {
                switchToInstance(item, /*openAdjacently=*/false);
            } else if (textId == R.string.instance_switcher_close_window) {
                // TODO: Implement undo/confirmation dialog
                Iterator<ListItem> it = mModelList.iterator();
                while (it.hasNext()) {
                    ListItem li = it.next();
                    int id = li.model.get(InstanceSwitcherItemProperties.INSTANCE_ID);
                    if (id == item.instanceId) {
                        mModelList.remove(li);
                        break;
                    }
                }
                mCloseAction.run(item.instanceId, item.taskId);
            }
        };

        PropertyModel model =
                new PropertyModel.Builder(InstanceSwitcherItemProperties.ALL_KEYS)
                        .with(InstanceSwitcherItemProperties.TITLE, title)
                        .with(InstanceSwitcherItemProperties.DESC, desc)
                        .with(InstanceSwitcherItemProperties.CURRENT, currentIndicator)
                        .with(InstanceSwitcherItemProperties.INSTANCE_ID, item.instanceId)
                        .with(InstanceSwitcherItemProperties.CLICK_LISTENER,
                                (view) -> switchToInstance(item, /*openAdjacently=*/true))
                        .with(InstanceSwitcherItemProperties.MORE_MENU,
                                () -> new BasicListMenu(mContext, moreMenu, moreMenuDelegate))
                        .build();
        LargeIconCallback callback = new LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(Bitmap icon, int fallbackColor,
                    boolean isFallbackColorDefault, @IconType int iconType) {
                if (icon == null) {
                    mIconGenerator.setBackgroundColor(fallbackColor);
                    icon = mIconGenerator.generateIconForUrl(item.url);
                } else {
                    icon = Bitmap.createScaledBitmap(
                            icon, mDisplayedIconSize, mDisplayedIconSize, true);
                }
                Drawable d = new BitmapDrawable(mContext.getResources(), icon);
                model.set(InstanceSwitcherItemProperties.FAVICON, d);
            }
        };
        mLargeIconBridge.getLargeIconForUrl(url, mMinIconSizeDp, callback);
        return model;
    }

    private void switchToInstance(InstanceInfo item, boolean openAdjacently) {
        if (item.type == InstanceInfo.Type.CURRENT || item.type == InstanceInfo.Type.ADJACENT) {
            // TODO: Show a toast.
            return;
        }
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        mOpenAction.run(item.instanceId, item.taskId, openAdjacently);
    }
}
