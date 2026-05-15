// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.AppMenuActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.HomeActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.tabswitcher.TabSwitcherActionButtonBinder;
import org.chromium.chrome.browser.ui.bottombar.BottomBarButtonManager.ActionConfig;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the bottom bar. */
@NullMarked
public class BottomBarCoordinator implements BottomBar, Destroyable {
    private final PropertyModel mModel;
    private final BottomBarMediator mMediator;
    private final BottomBarView mView;
    private final PropertyModelChangeProcessor<PropertyModel, BottomBarView, PropertyKey> mMcp;
    private final BottomBarButtonManager mButtonManager;

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
        mView =
                (BottomBarView)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.bottom_bar_layout, parent, false);

        boolean shouldIncludeHomeButton = BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled();
        boolean shouldIncludeGlicButton = AdaptiveToolbarFeatures.isGlicActionEnabled();
        List<ActionConfig> configs =
                createActionConfigs(mView, shouldIncludeHomeButton, shouldIncludeGlicButton);

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();

        mButtonManager =
                new BottomBarButtonManager(configs, actionRegistry, mModel, ActionId.NEW_TAB);

        mMediator =
                new BottomBarMediator(
                        mModel,
                        mButtonManager,
                        themeColorProvider,
                        tabSupplier,
                        homepageEnabledSupplier,
                        visibilityDelegate,
                        shouldIncludeHomeButton,
                        shouldIncludeGlicButton,
                        profileSupplier,
                        omniboxFocusStateSupplier);

        mMcp = PropertyModelChangeProcessor.create(mModel, mView, BottomBarViewBinder::bind);
    }

    private List<ActionConfig> createActionConfigs(
            BottomBarView view, boolean shouldIncludeHomeButton, boolean shouldIncludeGlicButton) {
        List<ActionConfig> configs = new ArrayList<>();

        if (shouldIncludeHomeButton) {
            BottomBarButtonContainer homeContainer =
                    view.getContainerForAction(ActionId.HOME_BUTTON);
            assert homeContainer != null : "Home button container not found";
            homeContainer.inflateStub();
            configs.add(
                    new ActionConfig(
                            ActionId.HOME_BUTTON,
                            homeContainer,
                            HomeActionButtonBinder::bind,
                            BottomBarProperties.IS_HOME_BUTTON_VISIBLE));
        }

        if (shouldIncludeGlicButton) {
            BottomBarButtonContainer extraContainer = view.getContainerForAction(ActionId.GLIC);
            assert extraContainer != null : "Extra button container not found";
            extraContainer.inflateStub();
            configs.add(
                    new ActionConfig(
                            ActionId.GLIC,
                            extraContainer,
                            GlicActionButtonBinder::bind,
                            BottomBarProperties.IS_GLIC_BUTTON_VISIBLE));
        }

        BottomBarButtonContainer newTabContainer = view.getContainerForAction(ActionId.NEW_TAB);
        assert newTabContainer != null : "New tab container not found";
        configs.add(
                new ActionConfig(
                        ActionId.NEW_TAB,
                        newTabContainer,
                        ActionButtonBinder::bind,
                        BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE));

        BottomBarButtonContainer tabSwitcherContainer =
                view.getContainerForAction(ActionId.TAB_SWITCHER);
        assert tabSwitcherContainer != null : "Tab switcher container not found";
        configs.add(
                new ActionConfig(
                        ActionId.TAB_SWITCHER,
                        tabSwitcherContainer,
                        TabSwitcherActionButtonBinder::bind,
                        BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE));

        if (BottomBarConfigUtils.shouldIncludeAppMenuButton()) {
            BottomBarButtonContainer menuContainer = view.getContainerForAction(ActionId.APP_MENU);
            assert menuContainer != null : "App menu container not found";
            menuContainer.inflateStub();
            menuContainer.setVisibility(View.VISIBLE);
            configs.add(
                    new ActionConfig(
                            ActionId.APP_MENU,
                            menuContainer,
                            AppMenuActionButtonBinder::bind,
                            BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE));
        }

        return configs;
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

    @Override
    public void destroy() {
        mMediator.destroy();
        mMcp.destroy();
        mButtonManager.destroy();
    }
}
