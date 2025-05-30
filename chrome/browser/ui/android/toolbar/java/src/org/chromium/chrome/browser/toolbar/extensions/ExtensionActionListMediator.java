// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Responsible for mediating external events like extension action changes and reflects all these
 * changes on the action button model.
 */
@NullMarked
class ExtensionActionListMediator implements Destroyable {
    private static final String TAG = "EALMediator";

    private final ModelList mModels;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private final Callback<Tab> mTabChangedCallback = this::onTabChanged;
    private final ActionsObserver mActionsObserver = new ActionsObserver();

    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    @Nullable private Profile mProfile;
    @Nullable private ExtensionActionsBridge mExtensionActionsBridge;
    @Nullable private Tab mCurrentTab;

    public ExtensionActionListMediator(
            ModelList models,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier) {
        mModels = models;
        mProfileSupplier = profileSupplier;
        mCurrentTabSupplier = currentTabSupplier;

        mProfileSupplier.addObserver(mProfileUpdatedCallback);
        mCurrentTabSupplier.addObserver(mTabChangedCallback);
    }

    @Override
    public void destroy() {
        if (mExtensionActionsBridge != null) {
            mExtensionActionsBridge.removeObserver(mActionsObserver);
        }
        mCurrentTabSupplier.removeObserver(mTabChangedCallback);
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);

        mCurrentTab = null;
        mExtensionActionsBridge = null;
        mProfile = null;

        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        if (mExtensionActionsBridge != null) {
            mExtensionActionsBridge.removeObserver(mActionsObserver);
        }
        mExtensionActionsBridge = null;
        mProfile = profile;

        if (mProfile != null) {
            mExtensionActionsBridge = ExtensionActionsBridge.get(mProfile);
            if (mExtensionActionsBridge != null) {
                mExtensionActionsBridge.addObserver(mActionsObserver);
            }
        }

        // Force clearing buttons even if the actions for the new profile are not available yet
        // because switching profiles usually results in a very different set of extension actions.
        mModels.clear();

        // If the current tab belongs to a different profile, onTabChanged will be called soon, so
        // do not update actions now to avoid duplicated updates.
        if (mCurrentTab != null && mCurrentTab.getProfile() != mProfile) {
            return;
        }

        maybeUpdateAllActions();
    }

    private void onTabChanged(Tab tab) {
        if (tab == mCurrentTab) {
            return;
        }
        if (tab == null) {
            // The current tab can be null when a non-tab UI is shown (e.g. tab switcher). In this
            // case, we do not bother refreshing actions as they're hidden anyway. We do not set
            // mCurrentTab to null because we can skip updating actions if the current tab is set
            // back to the previous tab.
            return;
        }

        mCurrentTab = tab;

        // If the tab belongs to a different profile, onProfileUpdated will be called soon, so
        // do not update actions now to avoid duplicated updates.
        if (tab.getProfile() != mProfile) {
            return;
        }

        maybeUpdateAllActions();
    }

    private void onPrimaryClick(View unused_buttonView, String actionId) {
        if (mExtensionActionsBridge == null || mCurrentTab == null) {
            return;
        }

        WebContents webContents = mCurrentTab.getWebContents();
        if (webContents == null) {
            // TODO(crbug.com/385985177): Revisit how to handle this case.
            return;
        }

        @ShowAction int showAction = mExtensionActionsBridge.runAction(actionId, webContents);
        switch (showAction) {
            case ShowAction.NONE:
                break;
            case ShowAction.SHOW_POPUP:
                Log.e(TAG, "Extension popups are not implemented yet");
                break;
            case ShowAction.TOGGLE_SIDE_PANEL:
                Log.e(TAG, "Extension side panels are not implemented yet");
                break;
        }
    }

    private void maybeUpdateAllActions() {
        if (mProfile == null || mExtensionActionsBridge == null || mCurrentTab == null) {
            mModels.clear();
            return;
        }

        if (!mExtensionActionsBridge.areActionsInitialized()) {
            // No need to update the model as ActionsObserver will be called back soon.
            return;
        }

        int tabId = mCurrentTab.getId();

        // TODO(crbug.com/385984462): Show pinned actions only. For now, we pretend that all actions
        // are pinned.
        String[] actionIds = mExtensionActionsBridge.getActionIds();

        List<ListItem> items = new ArrayList<>(actionIds.length);
        for (String actionId : actionIds) {
            ExtensionAction action = mExtensionActionsBridge.getAction(actionId, tabId);
            assert action != null;
            Bitmap icon = mExtensionActionsBridge.getActionIcon(actionId, tabId);
            assert icon != null;
            items.add(
                    new ModelListAdapter.ListItem(
                            ListItemType.EXTENSION_ACTION,
                            new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                                    .with(ExtensionActionButtonProperties.ICON, icon)
                                    .with(ExtensionActionButtonProperties.ID, action.getId())
                                    .with(
                                            ExtensionActionButtonProperties.ON_CLICK_LISTENER,
                                            (view) -> onPrimaryClick(view, actionId))
                                    .with(ExtensionActionButtonProperties.TITLE, action.getTitle())
                                    .build()));
        }
        mModels.set(items);
    }

    private class ActionsObserver implements ExtensionActionsBridge.Observer {
        @Override
        public void onActionAdded(String actionId) {
            // TODO(crbug.com/385984462): Update the added action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionRemoved(String actionId) {
            // TODO(crbug.com/385984462): Update the removed action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionUpdated(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionModelInitialized() {
            maybeUpdateAllActions();
        }

        @Override
        public void onPinnedActionsChanged() {
            // TODO(crbug.com/385984462): Update the pinned/unpinned actions only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionIconUpdated(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }
    }
}
