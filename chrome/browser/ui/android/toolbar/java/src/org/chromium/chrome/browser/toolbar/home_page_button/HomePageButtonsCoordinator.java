// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ACCESSIBILITY_TRAVERSAL_BEFORE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.CONTAINER_VISIBILITY;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ON_KEY_LISTENER;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.TRANSLATION_Y;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.HomeButtonDisplay;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** Root component for the Home button and NTP Customization button's container on the toolbar. */
@NullMarked
public class HomePageButtonsCoordinator implements HomeButtonDisplay {
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
            Supplier<@Nullable Profile> profileSupplier,
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

    // {@link HomeButtonDisplay} implementation.

    @Override
    public View getView() {
        return mHomePageButtonsContainerView;
    }

    @Override
    public void setVisibility(int visibility) {
        mModel.set(CONTAINER_VISIBILITY, visibility);
    }

    @Override
    public int getVisibility() {
        return mModel.get(CONTAINER_VISIBILITY);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        mModel.set(BUTTON_TINT_LIST, tint);
    }

    @Nullable
    @Override
    public ColorStateList getForegroundColor() {
        return mModel.get(BUTTON_TINT_LIST);
    }

    @Override
    public void setBackgroundResource(@DrawableRes int resId) {
        mModel.set(BUTTON_BACKGROUND, resId);
    }

    @Override
    public int getMeasuredWidth() {
        return mHomePageButtonsContainerView.getMeasuredWidth();
    }

    @Override
    public void updateState(
            @VisualState int toolbarVisualState,
            boolean isHomeButtonEnabled,
            boolean isHomepageNonNtp,
            boolean urlHasFocus) {
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

    @Override
    public void setAccessibilityTraversalBefore(@IdRes int viewId) {
        mModel.set(ACCESSIBILITY_TRAVERSAL_BEFORE, viewId);
    }

    @Override
    public void setTranslationY(float translationY) {
        mModel.set(TRANSLATION_Y, translationY);
    }

    @Override
    public void setClickable(boolean clickable) {
        mModel.set(IS_CLICKABLE, clickable);
    }

    @Override
    public void setOnKeyListener(View.OnKeyListener listener) {
        mModel.set(ON_KEY_LISTENER, listener);
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
