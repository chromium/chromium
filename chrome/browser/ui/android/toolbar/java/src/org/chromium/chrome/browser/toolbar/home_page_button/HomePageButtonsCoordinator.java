// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Root component for the Home button and NTP Customization button's container on the toolbar. */
@NullMarked
public class HomePageButtonsCoordinator {
    @IntDef({
        HomePageButtonsState.HIDDEN,
        HomePageButtonsState.SHOWING_HOME_BUTTON,
        HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON,
        HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON,
        HomePageButtonsState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HomePageButtonsState {
        int HIDDEN = 0;
        int SHOWING_HOME_BUTTON = 1;
        int SHOWING_CUSTOMIZATION_BUTTON = 2;
        int SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON = 3;
        int NUM_ENTRIES = 4;
    }

    private final HomePageButtonsContainerView mHomePageButtonsContainerView;

    private HomePageButtonsMediator mMediator;
    private @HomePageButtonsState int mHomePageButtonsState = HomePageButtonsState.NUM_ENTRIES;
    private PropertyModel mModel;

    /**
     * Creates a new instance of HomePageButtonsCoordinator
     *
     * @param context The Android context used for various view operations.
     * @param profileSupplier Supplier of the current profile of the User.
     * @param view An instance of {@link HomePageButtonsContainerView} to bind to.
     * @param onHomeButtonMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     * @param bottomSheetController The {@link BottomSheetController} to create the NTP
     *     Customization bottom sheet.
     * @param onHomeButtonClickListener Callback when home button is clicked.
     */
    public HomePageButtonsCoordinator(
            Context context,
            ObservableSupplier<Profile> profileSupplier,
            View view,
            Callback<Context> onHomeButtonMenuClickCallback,
            Supplier<Boolean> isHomepageMenuDisabledSupplier,
            BottomSheetController bottomSheetController,
            View.OnClickListener onHomeButtonClickListener) {
        mModel = new PropertyModel(HomePageButtonsProperties.ALL_KEYS);
        mHomePageButtonsContainerView = (HomePageButtonsContainerView) view;
        PropertyModelChangeProcessor.create(
                mModel, mHomePageButtonsContainerView, HomePageButtonsViewBinder::bind);

        mMediator =
                new HomePageButtonsMediator(
                        context,
                        profileSupplier,
                        mModel,
                        onHomeButtonMenuClickCallback,
                        isHomepageMenuDisabledSupplier,
                        bottomSheetController,
                        onHomeButtonClickListener);
    }

    /**
     * Updates the visibility and functionality of the home page buttons based on current home page
     * button state.
     *
     * @param toolbarVisualState The current visual state of the toolbar.
     * @param isHomeButtonEnabled True if the home button is enabled.
     * @param isHomepageNonNtp True if the current homepage is set to something other than the NTP.
     */
    public void updateButtonsState(
            @VisualState int toolbarVisualState,
            boolean isHomeButtonEnabled,
            boolean isHomepageNonNtp) {
        int homePageButtonsState;
        if (toolbarVisualState == VisualState.NEW_TAB_NORMAL
                || toolbarVisualState == VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO) {
            if (isHomeButtonEnabled && isHomepageNonNtp) {
                homePageButtonsState =
                        HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON;
            } else {
                homePageButtonsState = HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON;
            }
        } else {
            if (isHomeButtonEnabled) {
                homePageButtonsState = HomePageButtonsState.SHOWING_HOME_BUTTON;
            } else {
                homePageButtonsState = HomePageButtonsState.HIDDEN;
            }
        }

        if (mHomePageButtonsState == homePageButtonsState) {
            return;
        }

        mHomePageButtonsState = homePageButtonsState;
        mMediator.updateButtonsState(homePageButtonsState);
    }

    /** Returns the foreground color on the icons and label of the buttons. */
    public ColorStateList getButtonsForegroundColor() {
        return mModel.get(BUTTON_TINT_LIST);
    }

    /**
     * Updates the foreground color on the icons and label of the buttons to match the current
     * theme/website color.
     */
    public void setButtonsForegroundColor(ColorStateList colorStateList) {
        mModel.set(BUTTON_TINT_LIST, colorStateList);
    }

    /** Updates the background of the buttons to match the current address bar background. */
    public void setButtonsBackgroundResource(int backgroundResource) {
        mModel.set(BUTTON_BACKGROUND, backgroundResource);
    }

    void setMediatorForTesting(HomePageButtonsMediator mediator) {
        mMediator = mediator;
    }

    void setHomePageButtonsStateForTesting(int homePageButtonsState) {
        mHomePageButtonsState = homePageButtonsState;
    }

    void setModelForTesting(PropertyModel model) {
        mModel = model;
    }
}
