// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.HomeActionProperties;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ClickWithMetaStateCallback;
import org.chromium.ui.widget.RectProvider;

import java.util.function.Supplier;

/**
 * Root component for the {@link HomeButton} on the toolbar. Currently owns context menu for the
 * home button.
 */
// TODO(crbug.com/40676825): Fix the visibility bug on NTP.
@NullMarked
public class HomeButtonCoordinator extends ToolbarChildButton implements TintObserver {
    private static final int ID_SETTINGS = 0;

    private final Context mContext;
    private final HomeButton mHomeButton;
    private final Supplier<Boolean> mIsHomeButtonMenuDisabled;

    private final Callback<Context> mOnMenuClickCallback;
    private MVCListAdapter.@Nullable ModelList mMenuList;
    private @Nullable ListMenuDelegate mListMenuDelegate;

    private final @Nullable NullableObservableSupplier<PropertyModel> mHomeActionModelSupplier;
    private final Callback<@Nullable PropertyModel> mModelCallback = this::onModelChanged;
    private final ClickWithMetaStateCallback mClickCallback;

    /**
     * @param context The Android context used for various view operations.
     * @param homeButton The concrete {@link View} class for this MVC component.
     * @param clickCallback Callback invoked when button is clicked.
     * @param onMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     * @param themeColorProvider a provider that notifies about theme changes.
     * @param incognitoStateProvider a provider that notifies about incognito state changes.
     * @param homeActionModelSupplier Supplier for the Home action property model.
     */
    public HomeButtonCoordinator(
            Context context,
            View homeButton,
            ClickWithMetaStateCallback clickCallback,
            Callback<Context> onMenuClickCallback,
            Supplier<Boolean> isHomepageMenuDisabledSupplier,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable ActionRegistry actionRegistry) {
        super(context, themeColorProvider, incognitoStateProvider);
        mContext = context;
        // TODO(crbug.com/493273525): Make toolbar buttons use ActionRegistry.
        mHomeButton = (HomeButton) homeButton;
        mOnMenuClickCallback = onMenuClickCallback;
        mIsHomeButtonMenuDisabled = isHomepageMenuDisabledSupplier;
        mClickCallback = clickCallback;
        mHomeActionModelSupplier =
                actionRegistry != null ? actionRegistry.get(ActionId.HOME_BUTTON) : null;

        mHomeButton.setOnLongClickListener(this::onLongClickHomeButton);
        mHomeButton.setClickCallback(clickCallback);

        initializeMenuDelegate();
        if (mHomeActionModelSupplier != null) {
            mHomeActionModelSupplier.addSyncObserverAndCallIfNonNull(mModelCallback);
        }
    }

    private void initializeMenuDelegate() {
        mListMenuDelegate =
                new ListMenuDelegate() {
                    private @Nullable ListMenu mListMenu;
                    private @Nullable RectProvider mRectProvider;

                    @Override
                    public ListMenu getListMenu() {
                        if (mListMenu == null) {
                            mMenuList = new MVCListAdapter.ModelList();
                            mMenuList.add(
                                    new ListItemBuilder()
                                            .withTitleRes(R.string.options_homepage_edit_title)
                                            .withMenuId(ID_SETTINGS)
                                            .withStartIconRes(R.drawable.ic_edit_24dp)
                                            .build());
                            mListMenu =
                                    BrowserUiListMenuUtils.getBasicListMenu(
                                            mContext,
                                            mMenuList,
                                            (model, unusedView) ->
                                                    mOnMenuClickCallback.onResult(mContext));
                        }
                        return mListMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuButton) {
                        if (mRectProvider == null) {
                            // TODO(crbug.com/505443678): Polish for bottom bar item.
                            mRectProvider = MenuBuilderHelper.getRectProvider(listMenuButton);
                        }
                        return mRectProvider;
                    }
                };
        mHomeButton.setDelegate(mListMenuDelegate, false);
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(HomeActionProperties.CLICK_WITH_META_CALLBACK, mClickCallback);
        model.set(HomeActionProperties.LONG_PRESS_MENU_DELEGATE, mListMenuDelegate);
    }

    @VisibleForTesting
    boolean onLongClickHomeButton(View view) {
        if (view != mHomeButton || mIsHomeButtonMenuDisabled.get()) return false;
        mHomeButton.showMenu();
        return true;
    }

    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        mHomeButton.setHasSpaceToShow(hasSpaceToShow);
    }

    @Override
    public boolean isVisible() {
        return mHomeButton.getVisibility() == View.VISIBLE;
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mHomeButton, tint);
    }

    public MVCListAdapter.@Nullable ModelList getMenuForTesting() {
        return mMenuList;
    }
}
