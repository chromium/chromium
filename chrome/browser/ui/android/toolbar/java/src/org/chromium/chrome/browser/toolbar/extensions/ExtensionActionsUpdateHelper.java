// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;

/**
 * Responsible for monitoring {@link Tab}, and provides extension action updates to its delegate.
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

    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final ActionsUpdateDelegate mActionsUpdateDelegate;
    private final ModelList mModels;
    private final ExtensionActionsBridge mExtensionActionsBridge;

    private final Callback<@Nullable Tab> mTabChangedCallback = this::onTabChanged;
    private final ActionsObserver mActionsObserver = new ActionsObserver();

    @Nullable private Tab mCurrentTab;

    public ExtensionActionsUpdateHelper(
            ModelList models,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ActionsUpdateDelegate delegate) {
        mModels = models;
        mCurrentTabSupplier = currentTabSupplier;
        mActionsUpdateDelegate = delegate;
        mExtensionActionsBridge = new ExtensionActionsBridge(task, profile);

        mCurrentTabSupplier.addSyncObserverAndPostIfNonNull(mTabChangedCallback);
        mExtensionActionsBridge.addObserver(mActionsObserver);
    }

    private void maybeUpdateAllActions() {
        if (mCurrentTab == null) {
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

    private void onTabChanged(@Nullable Tab tab) {
        if (tab == mCurrentTab) {
            return;
        }

        // TODO(crbug.com/467472551): Move this to maybeUpdateAllActions().
        // However, simply moving it there will cause a crash on clicking an action button.
        mActionsUpdateDelegate.onUpdateStarted();

        if (tab == null) {
            // The current tab can be null when a non-tab UI is shown (e.g. tab switcher). In this
            // case, we do not bother refreshing actions as they're hidden anyway. We do not set
            // mCurrentTab to null because we can skip updating actions if the current tab is set
            // back to the previous tab.
            return;
        }

        mCurrentTab = tab;
        maybeUpdateAllActions();
    }

    /** Returns the current tab. */
    public @Nullable Tab getCurrentTab() {
        return mCurrentTab;
    }

    /** Return the `ExtensionActionsBridge` that corresponds to the window. */
    public ExtensionActionsBridge getExtensionActionsBridge() {
        return mExtensionActionsBridge;
    }

    @Override
    public void destroy() {
        mExtensionActionsBridge.removeObserver(mActionsObserver);
        mCurrentTabSupplier.removeObserver(mTabChangedCallback);
        mExtensionActionsBridge.destroy();

        mCurrentTab = null;
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
