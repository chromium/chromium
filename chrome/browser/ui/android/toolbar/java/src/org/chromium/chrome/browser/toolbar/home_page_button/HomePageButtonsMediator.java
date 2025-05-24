// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_DATA;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsCoordinator.HomePageButtonsState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

@NullMarked
public class HomePageButtonsMediator {
    private static final int ID_SETTINGS = 0;

    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Supplier<Boolean> mIsHomeButtonMenuDisabled;
    private final Callback<Context> mOnHomeButtonMenuClickCallback;
    private MVCListAdapter.@Nullable ModelList mHomeButtonMenuList;
    private @Nullable ListMenuDelegate mHomeButtonListMenuDelegate;
    private final PropertyModel mModel;
    private @Nullable HomePageButtonData mHomeButtonData;
    private @Nullable HomePageButtonData mNtpCustomizationButtonData;
    private final View.OnClickListener mOnHomeButtonClickListener;
    private final BottomSheetController mBottomSheetController;

    /**
     * Creates a new instance of HomePageButtonsMediator
     *
     * @param context The Android context used for various view operations.
     * @param profileSupplier Supplier of the current profile of the User.
     * @param model The model where HomePageButtonsContainerView is bind to.
     * @param onHomeButtonMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     * @param bottomSheetController The {@link BottomSheetController} to create the NTP
     *     Customization bottom sheet.
     * @param onHomeButtonClickListener Callback when home button is clicked.
     */
    public HomePageButtonsMediator(
            Context context,
            ObservableSupplier<Profile> profileSupplier,
            PropertyModel model,
            Callback<Context> onHomeButtonMenuClickCallback,
            Supplier<Boolean> isHomepageMenuDisabledSupplier,
            BottomSheetController bottomSheetController,
            View.OnClickListener onHomeButtonClickListener) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mModel = model;
        mOnHomeButtonMenuClickCallback = onHomeButtonMenuClickCallback;
        mIsHomeButtonMenuDisabled = isHomepageMenuDisabledSupplier;
        mBottomSheetController = bottomSheetController;
        mOnHomeButtonClickListener = onHomeButtonClickListener;

        prepareHomePageButtonsData();
    }

    void prepareHomePageButtonsData() {
        mHomeButtonData =
                new HomePageButtonData(
                        /* onClickListener= */ mOnHomeButtonClickListener,
                        /* onLongClickListener= */ view -> onLongClickHomeButton(view));
        mModel.set(BUTTON_DATA, new Pair<>(0, mHomeButtonData));

        mNtpCustomizationButtonData =
                new HomePageButtonData(
                        /* onClickListener= */ view ->
                                new NtpCustomizationCoordinator(
                                                mContext,
                                                mBottomSheetController,
                                                mProfileSupplier,
                                                NtpCustomizationCoordinator.BottomSheetType.MAIN)
                                        .showBottomSheet(),
                        /* onLongClickListener= */ null);
        mModel.set(BUTTON_DATA, new Pair<>(1, mNtpCustomizationButtonData));
    }

    void updateButtonsState(@HomePageButtonsState int homePageButtonsState) {
        switch (homePageButtonsState) {
            case HomePageButtonsState.HIDDEN:
                mModel.set(IS_CONTAINER_VISIBLE, false);
                return;
            case HomePageButtonsState.SHOWING_HOME_BUTTON:
                mModel.set(IS_CONTAINER_VISIBLE, true);
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(0, true));
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(1, false));
                return;
            case HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON:
                mModel.set(IS_CONTAINER_VISIBLE, true);
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(0, false));
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(1, true));
                return;
            case HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON:
                mModel.set(IS_CONTAINER_VISIBLE, true);
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(0, true));
                mModel.set(IS_BUTTON_VISIBLE, new Pair<>(1, true));
                return;
            default:
                assert false : "Home page button state not supported!";
        }
    }

    @VisibleForTesting
    boolean onLongClickHomeButton(View view) {
        if (mIsHomeButtonMenuDisabled.get()) return false;

        if (mHomeButtonListMenuDelegate == null) {
            RectProvider rectProvider = MenuBuilderHelper.getRectProvider(view);
            mHomeButtonMenuList = new MVCListAdapter.ModelList();
            mHomeButtonMenuList.add(
                    buildMenuListItem(
                            R.string.options_homepage_edit_title,
                            ID_SETTINGS,
                            R.drawable.ic_edit_24dp));
            BasicListMenu listMenu =
                    BrowserUiListMenuUtils.getBasicListMenu(
                            mContext,
                            mHomeButtonMenuList,
                            (model) -> mOnHomeButtonMenuClickCallback.onResult(mContext));
            mHomeButtonListMenuDelegate =
                    new ListMenuDelegate() {
                        @Override
                        public ListMenu getListMenu() {
                            return listMenu;
                        }

                        @Override
                        public RectProvider getRectProvider(View listMenuButton) {
                            return rectProvider;
                        }
                    };
            ((ListMenuButton) view).setDelegate(mHomeButtonListMenuDelegate, false);
        }
        ((ListMenuButton) view).showMenu();
        return true;
    }

    @Nullable HomePageButtonData getHomeButtonDataForTesting() {
        return mHomeButtonData;
    }

    @Nullable HomePageButtonData getNtpCustomizationButtonDataForTesting() {
        return mNtpCustomizationButtonData;
    }

    MVCListAdapter.@Nullable ModelList getMenuForTesting() {
        return mHomeButtonMenuList;
    }
}
