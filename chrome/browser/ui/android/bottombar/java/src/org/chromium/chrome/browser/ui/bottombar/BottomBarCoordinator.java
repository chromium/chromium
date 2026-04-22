// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the bottom bar. */
@NullMarked
public class BottomBarCoordinator implements BottomBar {
    private final PropertyModel mModel;
    private final BottomBarMediator mMediator;
    private final View mView;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mMcp;

    /**
     * @param parent The parent view to inflate the bottom bar into.
     * @param themeColorProvider The provider to observe theme changes from.
     * @param tabSupplier Supplier of the current tab.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     */
    public BottomBarCoordinator(
            ViewGroup parent,
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            BottomBarMediator.VisibilityDelegate visibilityDelegate) {
        mView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.bottom_bar_layout, parent, false);

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();
        mMediator =
                new BottomBarMediator(mModel, themeColorProvider, tabSupplier, visibilityDelegate);

        mMcp = PropertyModelChangeProcessor.create(mModel, mView, BottomBarViewBinder::bind);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void setParent(@Host int host) {}

    /** Destroys the coordinator and its components. */
    public void destroy() {
        mMediator.destroy();
        mMcp.destroy();
    }
}
