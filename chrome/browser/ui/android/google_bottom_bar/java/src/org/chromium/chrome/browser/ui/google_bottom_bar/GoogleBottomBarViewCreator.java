// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;

/** Builds the GoogleBottomBar view. */
public class GoogleBottomBarViewCreator {
    private final Context mContext;
    private final BottomBarConfig mConfig;
    private final GoogleBottomBarActionsHandler mActionsHandler;
    private final Supplier<PageInsightsCoordinator> mPageInsightsCoordinatorSupplier;
    private View mRootView;

    /**
     * Constructor.
     *
     * @param activity An Android activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param shareDelegateSupplier Supplier for the the share delegate.
     * @param pageInsightsCoordinatorSupplier Supplier for the page insights coordinator.
     * @param config Bottom bar configuration for the buttons that will be displayed.
     */
    public GoogleBottomBarViewCreator(
            Activity activity,
            Supplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<PageInsightsCoordinator> pageInsightsCoordinatorSupplier,
            BottomBarConfig config) {
        mContext = activity;
        mConfig = config;
        mActionsHandler =
                new GoogleBottomBarActionsHandler(
                        activity,
                        tabProvider,
                        shareDelegateSupplier,
                        pageInsightsCoordinatorSupplier);
        mPageInsightsCoordinatorSupplier = pageInsightsCoordinatorSupplier;
    }

    /**
     * Creates and initializes the bottom bar view.
     *
     * <p>This method dynamically determines the layout to use based on the presence of a spotlight
     * ID in the configuration. If a spotlight ID exists, a specialized layout is used; otherwise, a
     * standard even layout is used. TODO(b/290840238): Build view dynamically based on {@link
     * BottomBarConfig}.
     *
     * @return The created and initialized bottom bar view.
     */
    public View createGoogleBottomBarView() {
        mRootView =
                (mConfig.getSpotlightId() != null)
                        ? createGoogleBottomBarSpotlightLayoutView()
                        : createGoogleBottomBarEvenLayoutView();

        initButtons();

        return mRootView;
    }

    private void initButtons() {
        initButton(R.id.google_bottom_bar_save_button, ButtonId.SAVE);
        initButton(R.id.google_bottom_bar_share_button, ButtonId.SHARE);
        initButton(R.id.google_bottom_bar_page_insights_button, ButtonId.PIH_BASIC);
    }

    private void initButton(int viewId, @ButtonId int buttonConfigId) {
        assert mRootView != null;
        ImageView button = mRootView.findViewById(viewId);
        ButtonConfig buttonConfig = findButtonConfig(buttonConfigId);
        maybeUpdateButton(button, buttonConfig, /* isFirstTimeShown= */ true);
    }

    boolean updateBottomBarButton(@Nullable ButtonConfig buttonConfig) {
        if (buttonConfig == null) {
            return false;
        }
        ImageView button = mRootView.findViewById(buttonConfig.getId());
        return maybeUpdateButton(button, buttonConfig, /* isFirstTimeShown= */ false);
    }

    private @Nullable ButtonConfig findButtonConfig(@ButtonId int buttonId) {
        return mConfig.getButtonList().stream()
                .filter(config -> config.getId() == buttonId)
                .findFirst()
                .orElse(null);
    }

    private View createGoogleBottomBarEvenLayoutView() {
        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.EVEN_LAYOUT);
        return LayoutInflater.from(mContext).inflate(R.layout.google_bottom_bar_even, null);
    }

    private View createGoogleBottomBarSpotlightLayoutView() {
        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);
        return LayoutInflater.from(mContext).inflate(R.layout.google_bottom_bar_spotlight, null);
    }

    private boolean maybeUpdateButton(
            @Nullable ImageView button,
            @Nullable ButtonConfig buttonConfig,
            boolean isFirstTimeShown) {
        if (button == null || buttonConfig == null) {
            return false;
        }
        button.setId(buttonConfig.getId());
        button.setImageDrawable(buttonConfig.getIcon());
        button.setContentDescription(buttonConfig.getDescription());
        button.setOnClickListener(mActionsHandler.getClickListener(buttonConfig));

        int buttonEvent =
                GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(
                        mPageInsightsCoordinatorSupplier, buttonConfig);
        if (isFirstTimeShown) {
            GoogleBottomBarLogger.logButtonShown(buttonEvent);
        } else {
            GoogleBottomBarLogger.logButtonUpdated(buttonEvent);
        }
        return true;
    }
}
