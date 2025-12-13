// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/** Utility class for showing extension context menus. */
@NullMarked
public final class ExtensionActionContextMenuUtils {

    private ExtensionActionContextMenuUtils() {}

    /**
     * Creates and shows a ListMenu for the context menu.
     *
     * @param context The {@link Context} used to create the menu.
     * @param buttonView The {@link ListMenuButton} to anchor the menu to and set the delegate for.
     * @param bridge The {@link ExtensionActionContextMenuBridge} that provides the model and
     *     lifecycle.
     * @param rectProvider The {@link RectProvider} to use for positioning the menu.
     * @param rootView The root {@link View}, if required by the buttonView.
     */
    public static void showContextMenu(
            Context context,
            ListMenuButton buttonView,
            ExtensionActionContextMenuBridge bridge,
            RectProvider rectProvider,
            @Nullable View rootView) {
        ModelList modelList = bridge.getModelList();

        ListMenu.Delegate buttonDelegate =
                new ListMenu.Delegate() {
                    @Override
                    public void onItemSelected(PropertyModel model, View view) {
                        View.OnClickListener listener = model.get(CLICK_LISTENER);

                        if (listener != null) {
                            listener.onClick(view);
                        }
                    }
                };

        BasicListMenu basicListMenu =
                BrowserUiListMenuUtils.getBasicListMenu(context, modelList, buttonDelegate);

        ListMenu listMenu =
                new ListMenu() {
                    @Override
                    public View getContentView() {
                        return basicListMenu.getContentView();
                    }

                    @Override
                    public void addContentViewClickRunnable(Runnable runnable) {}

                    @Override
                    public int getMaxItemWidth() {
                        return basicListMenu.getMaxItemWidth();
                    }
                };

        basicListMenu.setupCallbacksRecursively(
                () -> {
                    buttonView.dismiss();
                },
                buttonView.getHost().getHierarchicalMenuController());

        ListMenuDelegate listDelegate =
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return listMenu;
                    }

                    @Override
                    public ListMenu getListMenuFromParentListItem(ListItem item) {
                        return BrowserUiListMenuUtils.getBasicListMenu(
                                context, ListMenuUtils.getModelListSubtree(item), buttonDelegate);
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuHostingView) {
                        return rectProvider;
                    }
                };
        buttonView.setDelegate(listDelegate, false);
        if (rootView != null) {
            buttonView.setRootView(rootView);
        }

        buttonView.addPopupListener(
                new ListMenuHost.PopupMenuShownListener() {
                    @Override
                    public void onPopupMenuShown() {}

                    @Override
                    public void onPopupMenuDismissed() {
                        bridge.destroy();
                        buttonView.removePopupListener(this);
                    }
                });

        buttonView.tryToFitLargestItem(true);

        buttonView.showMenu();
    }
}
