// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.ActionViewBinding;
import org.chromium.chrome.browser.ui.actions.AppMenuActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.HomeActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.tabswitcher.TabSwitcherActionButtonBinder;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the bottom bar. */
@NullMarked
public class BottomBarCoordinator implements BottomBar {
    private final PropertyModel mModel;
    private final BottomBarMediator mMediator;
    private final BottomBarView mView;
    private final PropertyModelChangeProcessor<PropertyModel, BottomBarView, PropertyKey> mMcp;
    private final List<ActionViewBinding> mBindings = new ArrayList<>();
    private final List<Integer> mRegisteredActionIds = new ArrayList<>();
    private final ActionRegistry mActionRegistry;
    private final PropertyObserver<PropertyKey> mModelObserver;

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
            BottomBarMediator.VisibilityDelegate visibilityDelegate,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mActionRegistry = actionRegistry;
        mView =
                (BottomBarView)
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
                        shouldIncludeHomeButton,
                        profileSupplier,
                        omniboxFocusStateSupplier);

        mMcp = PropertyModelChangeProcessor.create(mModel, mView, BottomBarViewBinder::bind);

        View newTabButton = mView.findViewById(R.id.new_tab_button);
        mRegisteredActionIds.add(ActionId.NEW_TAB);
        mBindings.add(new ActionViewBinding(actionRegistry.get(ActionId.NEW_TAB), newTabButton));

        mRegisteredActionIds.add(ActionId.TAB_SWITCHER);
        mBindings.add(
                new ActionViewBinding(
                        actionRegistry.get(ActionId.TAB_SWITCHER),
                        mView.findViewById(R.id.tab_switcher_button),
                        TabSwitcherActionButtonBinder::bind));

        if (AdaptiveToolbarFeatures.isGlicActionEnabled()) {
            BottomBarButtonContainer extraContainer =
                    mView.findViewById(R.id.extra_button_container);
            extraContainer.inflateStub();
            mRegisteredActionIds.add(ActionId.GLIC);
            mBindings.add(
                    new ActionViewBinding(
                            actionRegistry.get(ActionId.GLIC),
                            extraContainer.getTargetView(),
                            GlicActionButtonBinder::bind));
        }

        if (shouldIncludeHomeButton) {
            BottomBarButtonContainer homeContainer = mView.findViewById(R.id.home_button_container);
            homeContainer.inflateStub();
            mRegisteredActionIds.add(ActionId.HOME_BUTTON);
            mBindings.add(
                    new ActionViewBinding(
                            actionRegistry.get(ActionId.HOME_BUTTON),
                            homeContainer,
                            HomeActionButtonBinder::bind));
        }

        if (BottomBarConfigUtils.shouldIncludeAppMenuButton()) {
            BottomBarButtonContainer menuContainer =
                    mView.findViewById(R.id.app_menu_button_container);
            menuContainer.inflateStub();
            menuContainer.setVisibility(View.VISIBLE);
            mRegisteredActionIds.add(ActionId.APP_MENU);
            mBindings.add(
                    new ActionViewBinding(
                            actionRegistry.get(ActionId.APP_MENU),
                            menuContainer,
                            AppMenuActionButtonBinder::bind));
        }

        mModelObserver =
                (source, propertyKey) -> {
                    if (BottomBarProperties.COLOR_SCHEME == propertyKey) {
                        updateIconColors();
                    }
                };
        mModel.addObserver(mModelObserver);
        updateIconColors();
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public @ColorInt int getBackgroundColor() {
        return BottomBarUtils.getBottomBarBackgroundColor(
                mView.getContext(), mModel.get(BottomBarProperties.COLOR_SCHEME));
    }

    @Override
    public void setParent(@Host int host) {}

    /** Destroys the coordinator and its components. */
    public void destroy() {
        mMediator.destroy();
        mMcp.destroy();
        mModel.removeObserver(mModelObserver);
        for (ActionViewBinding binding : mBindings) {
            binding.destroy();
        }
        mBindings.clear();
    }

    private void updateIconColors() {
        @BrandedColorScheme int brandedColorScheme = mModel.get(BottomBarProperties.COLOR_SCHEME);
        ColorStateList tint =
                BottomBarUtils.getIconColorStateList(mView.getContext(), brandedColorScheme);
        for (int actionId : mRegisteredActionIds) {
            PropertyModel model = mActionRegistry.get(actionId).get();
            if (model != null) {
                model.set(ActionProperties.ICON_TINT, tint);
            }
        }
    }
}
