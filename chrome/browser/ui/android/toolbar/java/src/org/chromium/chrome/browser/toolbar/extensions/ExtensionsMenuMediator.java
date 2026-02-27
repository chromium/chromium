// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

import java.util.List;

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
    private final Profile mProfile;
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
            Profile profile,
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
        mProfile = profile;
        mMenuBridge = new ExtensionsMenuBridge(mTask, mProfile, /* observer= */ this);

        mMenuPropertyModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER,
                (buttonView, isChecked) -> mMenuBridge.onSiteSettingsToggleChanged(isChecked));

        if (mMenuBridge.isReady()) {
            onReady();
        }
    }

    private static class RelativeViewRectProvider extends RectProvider {
        private final View mAnchorView;
        private final View mParentView;
        private final int[] mAnchorLocation = new int[2];
        private final int[] mParentLocation = new int[2];

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
            mAnchorView.getLocationOnScreen(mAnchorLocation);
            mParentView.getLocationOnScreen(mParentLocation);

            int x = mAnchorLocation[0] - mParentLocation[0];
            int y = mAnchorLocation[1] - mParentLocation[1];

            mRect.set(x, y, x + mAnchorView.getWidth(), y + mAnchorView.getHeight());
            return mRect;
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
                        mTask, mProfile, actionId, webContents, ContextMenuSource.MENU_ITEM);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext,
                buttonView,
                contextMenuBridge,
                new RelativeViewRectProvider(buttonView, mRootView),
                /* dismissRunnable= */ null,
                mRootView);
    }

    /**
     * Updates the site settings toggle.
     *
     * @param siteSettingsState The site settings state to update to.
     */
    void updateSiteSettingsToggle(ExtensionsMenuTypes.SiteSettingsState siteSettingsState) {
        mMenuPropertyModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE,
                siteSettingsState.toggle.status != ExtensionsMenuTypes.ControlState.Status.HIDDEN);
        mMenuPropertyModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED,
                siteSettingsState.toggle.isOn);
        mMenuPropertyModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_LABEL, siteSettingsState.label);
    }

    /** Destroys the mediator. */
    @Override
    public void destroy() {
        mMenuBridge.destroy();
    }

    /**
     * Called when the native extensions menu model has changed. This method checks which page is
     * currently visible and pulls the relevant data from native to update the UI.
     */
    @Override
    public void onModelChanged() {
        // TODO(crbug.com/473213114): Implement data pull for site permissions page.
        // This will need to consider the event source (e.g., page navigation vs. action update)
        // to fetch and update the UI correctly, as their effects differ on the site permissions
        // page
        // and they will need to have different JNI observers.
        if (isMainPageVisible()) {
            ExtensionsMenuTypes.SiteSettingsState siteSettingsState =
                    mMenuBridge.getSiteSettingsState();
            updateSiteSettingsToggle(siteSettingsState);

            updateMenuEntries();
            return;
        }
    }

    /**
     * Pulls the list of menu entries from native and updates the action models list. Also updates
     * the zero state visibility.
     */
    private void updateMenuEntries() {
        mActionModels.clear();
        List<ExtensionsMenuTypes.MenuEntryState> entries = mMenuBridge.getMenuEntries();

        for (ExtensionsMenuTypes.MenuEntryState entry : entries) {
            boolean isActionPinned = entry.contextMenuButton.isOn;
            int contextMenuIcon =
                    isActionPinned ? R.drawable.ic_keep_24dp : R.drawable.ic_more_vert;
            PropertyModel model =
                    new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                            .with(ExtensionsMenuItemProperties.TITLE, entry.actionButton.text)
                            .with(
                                    ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ON_CLICK,
                                    (view) ->
                                            onContextMenuButtonClicked(
                                                    (ListMenuButton) view, entry.id))
                            .with(ExtensionsMenuItemProperties.ICON, entry.actionButton.icon)
                            .with(
                                    ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ICON,
                                    contextMenuIcon)
                            .build();

            mActionModels.add(new ListItem(0, model));
        }

        boolean isZeroState = entries.isEmpty();
        mMenuPropertyModel.set(ExtensionsMenuProperties.IS_ZERO_STATE, isZeroState);
    }

    @Override
    public void onActionIconUpdated(int actionIndex) {
        PropertyModel model = mActionModels.get(actionIndex).model;
        if (model == null) {
            return;
        }

        Bitmap icon = mMenuBridge.getActionIcon(actionIndex);
        model.set(ExtensionsMenuItemProperties.ICON, icon);
    }

    private boolean isMainPageVisible() {
        // TODO(crbug.com/473213114): Update this method when site permissions page is implemented.

        // For now, since there is only one page in Coordinator, always return true.
        return true;
    }

    /**
     * Called when the native side is ready with the menu data, which can happen on mediator
     * construction or by an observer called originated from the native side. Populates the action
     * models and updates the zero state visibility.
     */
    @Override
    public void onReady() {
        onModelChanged();
        mOnReady.run();
    }
}
