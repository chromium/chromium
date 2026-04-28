// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.ActionViewBinding;
import org.chromium.chrome.browser.ui.actions.HomeActionButtonBinder;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the bottom bar. */
@NullMarked
public class BottomBarCoordinator implements BottomBar {
    private final PropertyModel mModel;
    private final BottomBarMediator mMediator;
    private final View mView;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mMcp;
    private final List<ActionViewBinding> mBindings = new ArrayList<>();

    /**
     * @param parent The parent view to inflate the bottom bar into.
     * @param themeColorProvider The provider to observe theme changes from.
     * @param tabSupplier Supplier of the current tab.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     */
    public BottomBarCoordinator(
            ViewGroup parent,
            ActionRegistry actionRegistry,
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            BottomBarMediator.VisibilityDelegate visibilityDelegate) {
        mView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.bottom_bar_layout, parent, false);

        boolean shouldIncludeHomeButton = BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled();

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();
        mMediator =
                new BottomBarMediator(
                        mModel,
                        themeColorProvider,
                        tabSupplier,
                        homepageEnabledSupplier,
                        visibilityDelegate,
                        shouldIncludeHomeButton);

        mMcp = PropertyModelChangeProcessor.create(mModel, mView, BottomBarViewBinder::bind);

        mBindings.add(
                new ActionViewBinding(
                        actionRegistry.get(ActionId.NEW_TAB),
                        mView.findViewById(R.id.new_tab_button)));

        if (shouldIncludeHomeButton) {
            BottomBarButtonContainer homeContainer = mView.findViewById(R.id.home_button_container);
            homeContainer.inflateStub();
            mBindings.add(
                    new ActionViewBinding(
                            actionRegistry.get(ActionId.HOME_BUTTON),
                            homeContainer,
                            HomeActionButtonBinder::bind));
        }
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
        for (ActionViewBinding binding : mBindings) {
            binding.destroy();
        }
        mBindings.clear();
    }
}
