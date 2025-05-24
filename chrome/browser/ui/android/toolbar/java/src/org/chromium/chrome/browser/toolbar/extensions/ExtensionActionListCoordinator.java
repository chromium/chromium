// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ViewGroupAdapter;

/**
 * Root component for the extension action buttons. Exposes public API for external consumers to
 * interact with the buttons and affect their states.
 */
@NullMarked
public class ExtensionActionListCoordinator implements Destroyable {
    private final ExtensionActionListMediator mMediator;
    private final ViewGroupAdapter mAdapter;
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ExtensionActionListCoordinator(
            Context context,
            LinearLayout container,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier) {
        ModelList models = new ModelList();
        mMediator = new ExtensionActionListMediator(models, profileSupplier, currentTabSupplier);
        mAdapter =
                new ViewGroupAdapter.Builder(container, models)
                        .registerType(
                                ListItemType.EXTENSION_ACTION,
                                parent ->
                                        (ListMenuButton)
                                                LayoutInflater.from(context)
                                                        .inflate(
                                                                R.layout.extension_action_button,
                                                                parent,
                                                                /* attachToRoot= */ false),
                                ExtensionActionButtonViewBinder::bind)
                        .build();
    }

    @Override
    public void destroy() {
        mAdapter.destroy();
        mMediator.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
