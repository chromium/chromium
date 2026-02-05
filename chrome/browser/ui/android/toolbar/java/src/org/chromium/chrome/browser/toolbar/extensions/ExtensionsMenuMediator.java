// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/**
 * Mediator for the extensions menu. This class is responsible for listening to changes in the
 * extensions and updating the model accordingly.
 */
@NullMarked
class ExtensionsMenuMediator implements Destroyable, ExtensionsMenuBridge.Observer {
    private final ModelList mActionModels;
    private final Context mContext;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ExtensionsMenuBridge mMenuBridge;
    private final PropertyModel mMenuPropertyModel;
    private final Runnable mOnReady;
    private final ChromeAndroidTask mTask;
    private final View mRootView;

    /**
     * @param context The context to use.
     * @param task The task object.
     * @param currentTabSupplier The supplier for the current tab.
     * @param actionModels The model list to populate with extension actions.
     * @param rootView The root view of the menu.
     * @param onReady A runnable to run when the menu is ready to be shown.
     */
    public ExtensionsMenuMediator(
            Context context,
            ChromeAndroidTask task,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ModelList actionModels,
            PropertyModel propertyModel,
            View rootView,
            Runnable onReady) {
        mActionModels = actionModels;
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mOnReady = onReady;
        mMenuPropertyModel = propertyModel;
        mRootView = rootView;
        mTask = task;

        mMenuBridge = new ExtensionsMenuBridge(mTask, /* observer= */ this);
        if (mMenuBridge.isReady()) {
            onReady();
        }
    }

    private static class RelativeViewRectProvider extends RectProvider {
        private final View mAnchorView;
        private final View mParentView;

        /**
         * @param anchorView The view to be used as an anchor.
         * @param parentView The parent view to calculate relative coordinates.
         */
        RelativeViewRectProvider(View anchorView, View parentView) {
            mAnchorView = anchorView;
            mParentView = parentView;
        }

        /**
         * For {@link AnchoredPopupWindow} to correctly place nested popup windows, we have to make
         * sure to send coordinates relative to the main window of the application, not positions
         * relative to the parent popup window nor the screen.
         */
        @Override
        public Rect getRect() {
            int[] anchorLocation = new int[2];
            mAnchorView.getLocationOnScreen(anchorLocation);

            int[] parentLocation = new int[2];
            mParentView.getLocationOnScreen(parentLocation);

            int x = anchorLocation[0] - parentLocation[0];
            int y = anchorLocation[1] - parentLocation[1];

            return new Rect(x, y, x + mAnchorView.getWidth(), y + mAnchorView.getHeight());
        }
    }

    /**
     * Called when the context menu button for an extension is clicked.
     *
     * @param buttonView The button view that was clicked.
     * @param actionId The ID of the extension action.
     */
    private void onContextMenuButtonClicked(ListMenuButton buttonView, String actionId) {
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return;
        }

        ExtensionActionContextMenuBridge contextMenuBridge =
                new ExtensionActionContextMenuBridge(
                        mTask, actionId, webContents, ContextMenuSource.MENU_ITEM);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext,
                buttonView,
                contextMenuBridge,
                new RelativeViewRectProvider(buttonView, mRootView),
                mRootView);
    }

    /** Destroys the mediator. */
    @Override
    public void destroy() {
        mMenuBridge.destroy();
    }

    /**
     * Called when the native side is ready with the menu data, which can happen on mediator
     * construction or by an observer called originated from the native side. Populates the action
     * models and updates the zero state visibility.
     */
    @Override
    public void onReady() {
        // TODO(crbug.com/473213114): Currently getActions is returning an array that contains both
        // name and id, following [name1, id1, name2, id2, ...]. This is just an intermediary step
        // until we introduce a type that holds all the action information.
        String[] actions = mMenuBridge.getActions();
        for (int i = 0; i < actions.length; i += 2) {
            final String id = actions[i];
            final String name = actions[i + 1];

            PropertyModel model =
                    new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                            .with(ExtensionsMenuItemProperties.TITLE, name)
                            .with(
                                    ExtensionsMenuItemProperties.CLICK_LISTENER,
                                    (view) -> onContextMenuButtonClicked((ListMenuButton) view, id))
                            .build();

            mActionModels.add(new ListItem(0, model));
        }

        boolean isZeroState = actions.length == 0;
        mMenuPropertyModel.set(ExtensionsMenuProperties.IS_ZERO_STATE, isZeroState);
        mOnReady.run();
    }
}
