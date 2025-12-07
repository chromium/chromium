// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;

/**
 * Responsible for monitoring {@link Profile} and {@link Tab}, and provides extension action updates
 * to its delegate.
 */
@NullMarked
public class ExtensionActionsUpdateHelper implements Destroyable {
    /** A delegate to be notified of extension action updates. */
    public interface ActionsUpdateDelegate {
        /**
         * Called when the helper is about to update extension actions. The delegate can use this
         * method to make changes to its own states.
         */
        void onUpdateStarted();

        /**
         * Creates a model for a given extension action.
         *
         * @param extensionActionsBridge The bridge to get action properties.
         * @param tabId The ID of the tab to get action properties for.
         * @param actionId The ID of the action to create a model for.
         * @return A {@link ListItem} that represents the action.
         */
        ListItem createActionModel(
                ExtensionActionsBridge extensionActionsBridge, int tabId, String actionId);

        /** Called when the helper is finished updating extension actions. */
        void onUpdateFinished();
    }

    private final ObservableSupplier<@Nullable Profile> mProfileSupplier;
    private final ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;
    private final ActionsUpdateDelegate mActionsUpdateDelegate;
    private final ModelList mModels;

    private final Callback<@Nullable Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private final Callback<@Nullable Tab> mTabChangedCallback = this::onTabChanged;
    private final ActionsObserver mActionsObserver = new ActionsObserver();

    @Nullable private ExtensionActionsBridge mExtensionActionsBridge;
    @Nullable private Profile mProfile;
    @Nullable private Tab mCurrentTab;

    public ExtensionActionsUpdateHelper(
            ModelList models,
            ObservableSupplier<@Nullable Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            ActionsUpdateDelegate delegate) {
        mModels = models;
        mProfileSupplier = profileSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mActionsUpdateDelegate = delegate;

        mProfileSupplier.addObserver(mProfileUpdatedCallback);
        mCurrentTabSupplier.addObserver(mTabChangedCallback);
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
            items.add(
                    mActionsUpdateDelegate.createActionModel(
                            mExtensionActionsBridge, tabId, actionId));
        }
        mModels.set(items);

        mActionsUpdateDelegate.onUpdateFinished();
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mActionsUpdateDelegate.onUpdateStarted();

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

    private void onTabChanged(@Nullable Tab tab) {
        if (tab == mCurrentTab) {
            return;
        }

        mActionsUpdateDelegate.onUpdateStarted();

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

    /** Returns the profile. */
    public @Nullable Profile getProfile() {
        return mProfile;
    }

    /** Returns the current tab. */
    public @Nullable Tab getCurrentTab() {
        return mCurrentTab;
    }

    /** Return the `ExtensionActionsBridge` that corresponds to `mProfile`. */
    public @Nullable ExtensionActionsBridge getExtensionActionsBridge() {
        return mExtensionActionsBridge;
    }

    @Override
    public void destroy() {
        if (mExtensionActionsBridge != null) {
            mExtensionActionsBridge.removeObserver(mActionsObserver);
        }

        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
        mCurrentTabSupplier.removeObserver(mTabChangedCallback);

        mCurrentTab = null;
        mExtensionActionsBridge = null;
        mProfile = null;
    }

    private class ActionsObserver implements ExtensionActionsBridge.Observer {
        @Override
        public void onActionAdded(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionRemoved(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionUpdated(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionModelInitialized() {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onPinnedActionsChanged() {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }

        @Override
        public void onActionIconUpdated(String actionId) {
            // TODO(crbug.com/385984462): Update the updated action only.
            maybeUpdateAllActions();
        }
    }
}
