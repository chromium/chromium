// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
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
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.HomeActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.tabswitcher.TabSwitcherActionButtonBinder;
import org.chromium.chrome.browser.ui.bottombar.BottomBarButtonManager.ActionConfig;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.ui.modaldialog.ModalDialogManager;
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
    private final BottomBarPromoDialogCoordinator mPromoDialogCoordinator;

    /**
     * @param parent The parent view to inflate the bottom bar into.
     * @param actionRegistry The {@link ActionRegistry}.
     * @param themeColorProvider The provider to observe theme changes from.
     * @param tabSupplier Supplier of the current tab.
     * @param homepageEnabledSupplier Supplier of whether the homepage is enabled.
     * @param visibilityDelegate Delegate to handle compositor-level visibility changes.
     * @param profileSupplier Supplier of the current profile.
     * @param omniboxFocusStateSupplier Supplier of the omnibox focus state.
     * @param modalDialogManagerSupplier Supplier of the {@link ModalDialogManager}.
     */
    public BottomBarCoordinator(
            ViewGroup parent,
            ActionRegistry actionRegistry,
            ThemeColorProvider themeColorProvider,
            NullableObservableSupplier<Tab> tabSupplier,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            BottomBarMediator.VisibilityDelegate visibilityDelegate,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            NonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        Context context = parent.getContext();
        mView =
                (BottomBarView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.bottom_bar_layout, parent, false);

        boolean shouldIncludeHomeButton = BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled();
        List<ActionConfig> configs = createActionConfigs(mView, shouldIncludeHomeButton);

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();

        mButtonManager =
                new BottomBarButtonManager(configs, actionRegistry, mModel, ActionId.NEW_TAB);

        mPromoDialogCoordinator =
                new BottomBarPromoDialogCoordinator(
                        context, modalDialogManagerSupplier, profileSupplier);

        mMediator =
                new BottomBarMediator(
                        mModel,
                        mButtonManager,
                        themeColorProvider,
                        tabSupplier,
                        homepageEnabledSupplier,
                        visibilityDelegate,
                        shouldIncludeHomeButton,
                        profileSupplier,
                        omniboxFocusStateSupplier,
                        mPromoDialogCoordinator,
                        actionRegistry);
        mPromoDialogCoordinator.setListener(mMediator);

        mMcp = PropertyModelChangeProcessor.create(mModel, mView, BottomBarViewBinder::bind);
    }

    private List<ActionConfig> createActionConfigs(
            BottomBarView view, boolean shouldIncludeHomeButton) {
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

        BottomBarButtonContainer extraContainer = view.getContainerForAction(ActionId.GLIC);
        assert extraContainer != null : "Extra button container not found";
        configs.add(
                new ActionConfig(
                        ActionId.GLIC,
                        extraContainer,
                        GlicActionButtonBinder::bind,
                        BottomBarProperties.IS_GLIC_BUTTON_VISIBLE));

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
            menuContainer.inflateStub(ActionId.APP_MENU);
            menuContainer.setVisibility(View.VISIBLE);
            configs.add(
                    new ActionConfig(
                            ActionId.APP_MENU,
                            menuContainer,
                            ActionButtonBinder::bind,
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
        mPromoDialogCoordinator.destroy();
    }
}
