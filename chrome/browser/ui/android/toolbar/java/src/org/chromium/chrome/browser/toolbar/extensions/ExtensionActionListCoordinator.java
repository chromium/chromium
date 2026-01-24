// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Root component for the extension action buttons. Exposes public API for external consumers to
 * interact with the buttons and affect their states.
 */
@NullMarked
public class ExtensionActionListCoordinator implements Destroyable {
    private final ExtensionActionListRecyclerView mContainer;
    private final ModelList mModels;
    private final ExtensionActionListMediator mMediator;
    private final SimpleRecyclerViewAdapter mAdapter;
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ExtensionActionListCoordinator(
            Context context,
            ExtensionActionListRecyclerView container,
            WindowAndroid windowAndroid,
            ChromeAndroidTask task,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge) {
        mContainer = container;

        mModels = new ModelList();
        mMediator =
                new ExtensionActionListMediator(
                        context,
                        windowAndroid,
                        mModels,
                        task,
                        currentTabSupplier,
                        container,
                        extensionsToolbarBridge);

        mAdapter = new SimpleRecyclerViewAdapter(mModels);
        mAdapter.registerType(
                ListItemType.EXTENSION_ACTION,
                parent ->
                        (ListMenuButton)
                                LayoutInflater.from(context)
                                        .inflate(
                                                R.layout.extension_action_button,
                                                parent,
                                                /* attachToRoot= */ false),
                ExtensionActionButtonViewBinder::bind);

        mContainer.setLayoutManager(
                new LinearLayoutManager(
                        context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false) {
                    @Override
                    public boolean canScrollHorizontally() {
                        return false;
                    }
                });
        mContainer.setAdapter(mAdapter);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /** Performs a click on the button for the given action. */
    public void click(String actionId) {
        for (int i = 0; i < mModels.size(); i++) {
            if (mModels.get(i).model.get(ExtensionActionButtonProperties.ID).equals(actionId)) {
                RecyclerView.ViewHolder holder = mContainer.findViewHolderForAdapterPosition(i);
                if (holder == null) {
                    // TODO(crbug.com/478113313): Pop out action in so that the view exists for
                    // non-pinned actions.
                    return;
                }

                holder.itemView.performClick();
                return;
            }
        }
    }
}
