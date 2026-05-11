// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
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
    private final PropertyModel mMainPageModel;
    private final PropertyModel mSitePermissionsPageModel;
    private final Runnable mOnReady;
    private final Runnable mOnDismissMenu;
    private final ChromeAndroidTask mTask;
    private final Profile mProfile;
    private final TabCreator mTabCreator;

    /**
     * @param context The context to use.
     * @param task The task object.
     * @param profile The current profile.
     * @param currentTabSupplier The supplier for the current tab.
     * @param tabCreator The tab creator to use.
     * @param actionModels The model list to populate with extension actions.
     * @param mainPageModel The property model for the menu.
     * @param sitePermissionsPropertyModel The property model for the site permissions page.
     * @param dismissRunnable A runnable to dismiss the menu.
     * @param onReady A runnable to run when the menu is ready to be shown.
     */
    public ExtensionsMenuMediator(
            Context context,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            ExtensionsToolbarBridge toolbarBridge,
            ModelList actionModels,
            PropertyModel mainPageModel,
            PropertyModel sitePermissionsPropertyModel,
            Runnable onDismissMenu,
            Runnable onReady) {
        mActionModels = actionModels;
        mContext = context;
        mCurrentTabSupplier = currentTabSupplier;
        mOnDismissMenu = onDismissMenu;
        mOnReady = onReady;
        mTabCreator = tabCreator;
        mTask = task;
        mProfile = profile;
        mMenuBridge =
                new ExtensionsMenuBridge(mTask, mProfile, toolbarBridge, /* observer= */ this);

        mMainPageModel = mainPageModel;
        mSitePermissionsPageModel = sitePermissionsPropertyModel;

        if (mMenuBridge.isReady()) {
            onReady();
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
                new ViewRectProvider(buttonView),
                /* dismissRunnable= */ null);
    }

    /** Called when the allow extension button is clicked. */
    public void onAllowExtensionClicked(String extensionId) {
        mMenuBridge.onAllowExtensionClicked(extensionId);
    }

    /**
     * Called when the back button in the site permissions page is clicked, navigates back to the
     * main page.
     */
    public void onBackButtonClicked() {
        mMainPageModel.set(
                ExtensionsMenuProperties.CURRENT_PAGE, ExtensionsMenuProperties.Page.MAIN);
        // Refresh the main page, since there may have been changes while the site permissions page
        // was opened.
        onModelChanged();
    }

    /** Called when the dismiss extension button is clicked. */
    public void onDismissExtensionClicked(String extensionId) {
        mMenuBridge.onDismissExtensionClicked(extensionId);
    }

    /** Called when the discover extensions button is clicked. */
    public void onDiscoverExtensionsClicked() {
        openUrlFromMenu(UrlConstants.CHROME_WEBSTORE_URL);
    }

    /** Called when the manage extensions button is clicked. */
    public void onManageExtensionsClicked() {
        openUrlFromMenu(UrlConstants.CHROME_EXTENSIONS_URL);
    }

    /** Called when the manage extension button is clicked. */
    public void onManageThisExtensionClicked() {
        assert getCurrentPage() == ExtensionsMenuProperties.Page.SITE_PERMISSIONS;
        String extensionId =
                mSitePermissionsPageModel.get(SitePermissionsPageProperties.EXTENSION_ID);
        openUrlFromMenu(UrlConstants.CHROME_EXTENSIONS_ID_URL + extensionId);
    }

    /** Called when the reload page button is clicked. */
    public void onReloadPageButtonClicked() {
        mMenuBridge.onReloadPageButtonClicked();
    }

    /** Called when the site settings toggle is clicked. */
    public void onSiteSettingsToggleChanged(boolean isChecked) {
        mMenuBridge.onSiteSettingsToggleChanged(isChecked);
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
        if (getCurrentPage() == ExtensionsMenuProperties.Page.MAIN) {
            int optionalSection = mMenuBridge.getOptionalSection();
            mMainPageModel.set(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE, optionalSection);

            if (optionalSection == ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS) {
                mMainPageModel.set(
                        ExtensionsMenuProperties.HOST_ACCESS_REQUESTS,
                        mMenuBridge.getHostAccessRequests());
            } else {
                mMainPageModel.set(
                        ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, new ArrayList<>());
            }

            updateMenuEntries();
        } else if (getCurrentPage() == ExtensionsMenuProperties.Page.SITE_PERMISSIONS) {
            String extensionId =
                    mSitePermissionsPageModel.get(SitePermissionsPageProperties.EXTENSION_ID);
            updateSitePermissionsPage(extensionId);
        }
    }

    @Override
    public void onActionAdded(int actionIndex) {
        // A new toolbar action only affects the main page.
        if (getCurrentPage() != ExtensionsMenuProperties.Page.MAIN) {
            return;
        }

        ExtensionsMenuTypes.MenuEntryState entry = mMenuBridge.getMenuEntry(actionIndex);
        mActionModels.add(actionIndex, createMenuItem(entry));

        updateZeroState();
    }

    @Override
    public void onActionIconUpdated(int actionIndex) {
        if (getCurrentPage() == ExtensionsMenuProperties.Page.MAIN) {
            // Update the icon for the extension entry in the main page.
            PropertyModel model = mActionModels.get(actionIndex).model;
            if (model == null) {
                return;
            }

            Bitmap icon = mMenuBridge.getActionIcon(actionIndex);
            model.set(ExtensionsMenuItemProperties.ICON, icon);
            return;
        }

        // Do nothing when the site permissions page is opened for a different
        // extension.
        ExtensionsMenuTypes.MenuEntryState entry = mMenuBridge.getMenuEntry(actionIndex);
        if (!isSitePermissionsPageOpenedFor(entry)) {
            return;
        }

        // Update the icon for the extension's site permission page
        Bitmap icon = mMenuBridge.getActionIcon(actionIndex);
        mSitePermissionsPageModel.set(SitePermissionsPageProperties.EXTENSION_ICON, icon);
    }

    @Override
    public void onActionRemoved(int actionIndex) {
        if (getCurrentPage() == ExtensionsMenuProperties.Page.MAIN) {
            // Remove the menu entry for the extension when main page is opened.
            assert actionIndex >= 0 && actionIndex < mActionModels.size();
            mActionModels.removeAt(actionIndex);

            updateZeroState();
            return;
        }

        // Do nothing when the site permissions page is opened for a different
        // extension.
        ExtensionsMenuTypes.MenuEntryState entry = mMenuBridge.getMenuEntry(actionIndex);
        if (!isSitePermissionsPageOpenedFor(entry)) {
            return;
        }

        // Return to the main page when extension is removed and had the site permissions page
        // opened.
        mMainPageModel.set(
                ExtensionsMenuProperties.CURRENT_PAGE, ExtensionsMenuProperties.Page.MAIN);
    }

    @Override
    public void onActionUpdated(int newIndex) {
        if (getCurrentPage() == ExtensionsMenuProperties.Page.MAIN) {
            ExtensionsMenuTypes.MenuEntryState entry = mMenuBridge.getMenuEntry(newIndex);

            // Find the old index for the model corresponding to the updated action.
            int oldIndex = -1;
            for (int i = 0; i < mActionModels.size(); i++) {
                String modelId =
                        mActionModels.get(i).model.get(ExtensionsMenuItemProperties.EXTENSION_ID);
                if (modelId.equals(entry.id)) {
                    oldIndex = i;
                    break;
                }
            }
            assert oldIndex != -1
                    : "Action model with ID " + entry.id + " should exist in mActionModels.";

            // Update the menu item model.
            ListItem item = mActionModels.get(oldIndex);
            updateMenuItem(item.model, entry);

            // Update position if the index changed.
            if (oldIndex != newIndex) {
                mActionModels.removeAt(oldIndex);
                mActionModels.add(newIndex, item);
            }

            // An action update can change the state of the site settings toggle.
            updateSiteSettingsToggle();
            return;
        }

        // Do nothing when the site permissions page is opened for a different
        // extension.
        ExtensionsMenuTypes.MenuEntryState entry = mMenuBridge.getMenuEntry(newIndex);
        if (!isSitePermissionsPageOpenedFor(entry)) {
            return;
        }

        // Update the site permissions page for the extension.
        // TODO(crbug.com/473213114): If the extension no longer has site access, which can happen
        // during an update, the site permissions page should no longer be visible and we should
        // go back to the main page.
        updateSitePermissionsPage(entry.id);
    }

    /** Called when the list of pinned actions changed. */
    @Override
    public void onPinnedActionsChanged() {
        updateMenuEntries();
    }

    /** Called when a host access request has been added. */
    @Override
    public void onHostAccessRequestAdded(String extensionId) {
        updateHostAccessRequests();
    }

    /** Called when a host access request has been updated. */
    @Override
    public void onHostAccessRequestUpdated(String extensionId) {
        updateHostAccessRequests();
    }

    /** Called when a host access request has been removed. */
    @Override
    public void onHostAccessRequestRemoved(String extensionId) {
        updateHostAccessRequests();
    }

    /** Called when all host access requests have been cleared. */
    @Override
    public void onHostAccessRequestsCleared() {
        updateHostAccessRequests();
    }

    /** Called when the show requests toggle for an extension changed. */
    @Override
    public void onShowHostAccessRequestsInToolbarChanged(String extensionId) {
        if (getCurrentPage() != ExtensionsMenuProperties.Page.SITE_PERMISSIONS) {
            return;
        }

        String currentExtensionId =
                mSitePermissionsPageModel.get(SitePermissionsPageProperties.EXTENSION_ID);
        if (extensionId.equals(currentExtensionId)) {
            updateSitePermissionsPage(extensionId);
        }
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

    /**
     * Creates a menu item for an extension action.
     *
     * @param entry The state of the menu entry to create.
     * @return The created list item.
     */
    private ListItem createMenuItem(ExtensionsMenuTypes.MenuEntryState entry) {
        PropertyModel model =
                new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                        .with(ExtensionsMenuItemProperties.EXTENSION_ID, entry.id)
                        .with(
                                ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ON_CLICK,
                                (view) ->
                                        onContextMenuButtonClicked((ListMenuButton) view, entry.id))
                        .with(
                                ExtensionsMenuItemProperties.PRIMARY_ACTION_ON_CLICK,
                                (view) -> mMenuBridge.executeAction(entry.id))
                        .with(
                                ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_ON_CLICK,
                                (buttonView, isOn) ->
                                        mMenuBridge.onExtensionToggleSelected(entry.id, isOn))
                        .with(
                                ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK,
                                (view) -> onSitePermissionsButtonClicked(entry.id))
                        .build();
        updateMenuItem(model, entry);
        return new ListItem(0, model);
    }

    /** Returns the current page being displayed in the extensions menu. */
    private @ExtensionsMenuProperties.Page int getCurrentPage() {
        return mMainPageModel.get(ExtensionsMenuProperties.CURRENT_PAGE);
    }

    /**
     * Returns whether the site permissions page is currently opened for the given extension entry.
     */
    private boolean isSitePermissionsPageOpenedFor(ExtensionsMenuTypes.MenuEntryState entry) {
        if (getCurrentPage() != ExtensionsMenuProperties.Page.SITE_PERMISSIONS) {
            return false;
        }

        String extensionId =
                mSitePermissionsPageModel.get(SitePermissionsPageProperties.EXTENSION_ID);
        return entry.id.equals(extensionId);
    }

    /**
     * Navigates to the site permissions page for the given extension.
     *
     * @param extensionId The ID of the extension to show permissions for.
     */
    private void onSitePermissionsButtonClicked(String extensionId) {
        mSitePermissionsPageModel.set(SitePermissionsPageProperties.EXTENSION_ID, extensionId);

        updateSitePermissionsPage(extensionId);

        // Set current page to site permissions page.
        mMainPageModel.set(
                ExtensionsMenuProperties.CURRENT_PAGE,
                ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
    }

    private void openUrlFromMenu(String url) {
        mOnDismissMenu.run();

        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        mTabCreator.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
    }

    /**
     * Updates the host access requests list in the PropertyModel only if the host access requests
     * section is currently visible to the user.
     */
    private void updateHostAccessRequests() {
        // Site access requests only affect the main page.
        if (getCurrentPage() != ExtensionsMenuProperties.Page.MAIN) {
            return;
        }

        int currentSection = mMainPageModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE);
        if (currentSection == ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS) {
            mMainPageModel.set(
                    ExtensionsMenuProperties.HOST_ACCESS_REQUESTS,
                    mMenuBridge.getHostAccessRequests());
        }
    }

    /** Updates the itemModel for an extension menu entry according to the given state. */
    private void updateMenuItem(
            PropertyModel itemModel, ExtensionsMenuTypes.MenuEntryState itemState) {
        itemModel.set(ExtensionsMenuItemProperties.TITLE, itemState.actionButton.text);
        itemModel.set(ExtensionsMenuItemProperties.ICON, itemState.actionButton.icon);
        itemModel.set(ExtensionsMenuItemProperties.IS_PINNED, itemState.contextMenuButton.isOn);
        itemModel.set(
                ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ACCESSIBLE_NAME,
                itemState.contextMenuButton.accessibleName);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_CHECKED,
                itemState.siteAccessToggle.isOn);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS,
                itemState.siteAccessToggle.status);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_TOOLTIP,
                itemState.siteAccessToggle.tooltipText);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME,
                itemState.sitePermissionsButton.accessibleName);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS,
                itemState.sitePermissionsButton.status);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TEXT,
                itemState.sitePermissionsButton.text);
        itemModel.set(
                ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TOOLTIP,
                itemState.sitePermissionsButton.tooltipText);
        itemModel.set(ExtensionsMenuItemProperties.IS_ENTERPRISE, itemState.isEnterprise);
    }

    /**
     * Updates the menu items by pulling the list of menu entries from native and updating the
     * action models list. Also updates the zero state visibility.
     */
    private void updateMenuEntries() {
        List<ExtensionsMenuTypes.MenuEntryState> entries = mMenuBridge.getMenuEntries();

        if (mActionModels.size() != entries.size()) {
            // If sizes mismatch (e.g., initial load), clear and rebuild.
            reconstructModel(entries);
        } else {
            // Update items in-place to keep the {@link ListItem} instances.
            for (int i = 0; i < entries.size(); i++) {
                ExtensionsMenuTypes.MenuEntryState entry = entries.get(i);
                ListItem item = mActionModels.get(i);

                String currentId = item.model.get(ExtensionsMenuItemProperties.EXTENSION_ID);
                if (!currentId.equals(entry.id)) {
                    // In case the item order is different, clear and rebuild.
                    reconstructModel(entries);
                    break;
                }

                updateMenuItem(item.model, entry);
            }
        }

        updateZeroState();
    }

    /** Resets and reconstructs {@link mActionModels}. */
    private void reconstructModel(List<ExtensionsMenuTypes.MenuEntryState> entries) {
        mActionModels.clear();
        for (ExtensionsMenuTypes.MenuEntryState entry : entries) {
            mActionModels.add(createMenuItem(entry));
        }
    }

    /**
     * Updates the site permissions page for the given extension.
     *
     * @param extensionId The ID of the extension to show permissions for.
     */
    private void updateSitePermissionsPage(String extensionId) {
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                mMenuBridge.getExtensionSitePermissionsState(extensionId);

        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.EXTENSION_NAME, sitePermissionsState.extensionName);
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.EXTENSION_ICON, sitePermissionsState.extensionIcon);

        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.ON_CLICK_STATE, sitePermissionsState.onClickOption);
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.ON_SITE_STATE, sitePermissionsState.onSiteOption);
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.ON_ALL_SITES_STATE,
                sitePermissionsState.onAllSitesOption);
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.ON_SITE_ACCESS_SELECTED_LISTENER,
                (siteAccess) -> mMenuBridge.onExtensionSiteAccessSelected(extensionId, siteAccess));

        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CHECKED,
                sitePermissionsState.showRequestsToggle.isOn);
        mSitePermissionsPageModel.set(
                SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CLICK_LISTENER,
                (buttonView, isChecked) ->
                        mMenuBridge.onShowRequestsTogglePressed(extensionId, isChecked));
    }

    /** Updates the zero state visibility. */
    private void updateZeroState() {
        boolean isZeroState = mActionModels.size() == 0;
        mMainPageModel.set(ExtensionsMenuProperties.IS_ZERO_STATE, isZeroState);
        if (isZeroState) {
            // If we are in zero state, hide the site settings container to keep the empty state
            // clean.
            mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, false);
            // We also hide the discover extensions button in the main page, as there is already an
            // open web store button present in the zero state view.
            mMainPageModel.set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, false);
        } else {
            mMainPageModel.set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, true);
            updateSiteSettingsToggle();
        }
    }

    /** Updates the site settings toggle. */
    @VisibleForTesting
    void updateSiteSettingsToggle() {
        ExtensionsMenuTypes.SiteSettingsState siteSettingsState =
                mMenuBridge.getSiteSettingsState();
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        mMainPageModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE,
                siteSettingsState.toggle.status != ExtensionsMenuTypes.ControlState.Status.HIDDEN);
        mMainPageModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED,
                siteSettingsState.toggle.isOn);
        mMainPageModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_TOOLTIP,
                siteSettingsState.toggle.tooltipText);
        mMainPageModel.set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, siteSettingsState.label);
        mMainPageModel.set(
                ExtensionsMenuProperties.SITE_SETTINGS_INFO_ICON_VISIBLE,
                siteSettingsState.hasTooltip);
    }
}
