// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the extensions menu. This class is responsible for listening to changes in the
 * extensions and updating the model accordingly.
 */
@NullMarked
class ExtensionsMenuMediator implements Destroyable {
    private final ActionsUpdateDelegate mActionsUpdateDelegate = new ActionsUpdateDelegate();
    private final Runnable mOnUpdateFinishedRunnable;
    private final ExtensionActionsUpdateHelper mExtensionActionsUpdateHelper;

    public ExtensionsMenuMediator(
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            ModelList extensionModels,
            Runnable onUpdateFinishedRunnable) {
        mOnUpdateFinishedRunnable = onUpdateFinishedRunnable;

        mExtensionActionsUpdateHelper =
                new ExtensionActionsUpdateHelper(
                        extensionModels,
                        profileSupplier,
                        currentTabSupplier,
                        mActionsUpdateDelegate);
    }

    @Override
    public void destroy() {
        mExtensionActionsUpdateHelper.destroy();
    }

    private class ActionsUpdateDelegate
            implements ExtensionActionsUpdateHelper.ActionsUpdateDelegate {
        @Override
        public void onUpdateStarted() {}

        @Override
        public ListItem createActionModel(
                ExtensionActionsBridge extensionActionsBridge, int tabId, String actionId) {
            ExtensionAction action = extensionActionsBridge.getAction(actionId, tabId);
            assert action != null;
            Bitmap icon = extensionActionsBridge.getActionIcon(actionId, tabId);
            assert icon != null;
            return new ModelListAdapter.ListItem(
                    0,
                    new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                            .with(ExtensionsMenuItemProperties.TITLE, action.getTitle())
                            .with(ExtensionsMenuItemProperties.ICON, icon)
                            .build());
        }

        @Override
        public void onUpdateFinished() {
            mOnUpdateFinishedRunnable.run();
        }
    }
}
