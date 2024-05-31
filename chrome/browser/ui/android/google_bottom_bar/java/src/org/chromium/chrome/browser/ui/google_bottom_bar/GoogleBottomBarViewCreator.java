// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

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
    private ViewGroup mRootView;

    /**
     * Constructor.
     *
     * @param activity An Android activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param shareDelegateSupplier Supplier for the the share delegate.
     * @param pageInsightsCoordinatorSupplier Supplier for the page insights coordinator
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
        return mRootView;
    }

    boolean updateBottomBarButton(@Nullable ButtonConfig buttonConfig) {
        if (buttonConfig == null) {
            return false;
        }
        ImageButton button = mRootView.findViewById(buttonConfig.getId());
        return button == null
                ? false
                : updateButton(button, buttonConfig, /* isFirstTimeShown= */ false);
    }

    void logButtons() {
        for (ButtonConfig buttonConfig : mConfig.getButtonList()) {
            View button = mRootView.findViewById(buttonConfig.getId());
            if (button != null) {
                int buttonEvent =
                        GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(
                                mPageInsightsCoordinatorSupplier, buttonConfig);
                GoogleBottomBarLogger.logButtonShown(buttonEvent);
            }
        }
    }

    private @Nullable ButtonConfig findButtonConfig(@ButtonId int buttonId) {
        return mConfig.getButtonList().stream()
                .filter(config -> config.getId() == buttonId)
                .findFirst()
                .orElse(null);
    }

    private ViewGroup createGoogleBottomBarEvenLayoutView() {
        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.EVEN_LAYOUT);

        LinearLayout rootView = getEvenLayoutRoot();
        for (BottomBarConfig.ButtonConfig buttonConfig : mConfig.getButtonList()) {
            createButtonAndAddToParent(
                    rootView,
                    R.layout.bottom_bar_button_even_layout,
                    buttonConfig,
                    /* addAsFirst= */ false);
        }
        return (ViewGroup) rootView;
    }

    private ViewGroup createGoogleBottomBarSpotlightLayoutView() {
        GoogleBottomBarLogger.logCreatedEvent(GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);

        LinearLayout rootView = getSpotlightLayoutRoot();

        // Set up Spotlight Button
        createButtonAndAddToParent(
                rootView,
                R.layout.bottom_bar_button_spotlight_layout,
                findButtonConfig(mConfig.getSpotlightId()),
                /* addAsFirst= */ true);

        LinearLayout nonSpotlightContainer =
                rootView.findViewById(R.id.google_bottom_bar_non_spotlit_buttons_container);
        for (BottomBarConfig.ButtonConfig buttonConfig : mConfig.getButtonList()) {
            if (buttonConfig.getId() == mConfig.getSpotlightId()) continue;
            createButtonAndAddToParent(
                    nonSpotlightContainer,
                    R.layout.bottom_bar_button_non_spotlight_layout,
                    buttonConfig,
                    /* addAsFirst= */ false);
        }
        return (ViewGroup) rootView;
    }

    private boolean updateButton(
            @Nullable ImageButton button,
            @Nullable ButtonConfig buttonConfig,
            boolean isFirstTimeShown) {
        button.setId(buttonConfig.getId());
        button.setImageDrawable(buttonConfig.getIcon());
        button.setContentDescription(buttonConfig.getDescription());
        button.setOnClickListener(mActionsHandler.getClickListener(buttonConfig));
        button.setVisibility(View.VISIBLE);

        if (!isFirstTimeShown) {
            int buttonEvent =
                    GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(
                            mPageInsightsCoordinatorSupplier, buttonConfig);
            GoogleBottomBarLogger.logButtonUpdated(buttonEvent);
        }
        return true;
    }

    private void createButtonAndAddToParent(
            LinearLayout parent,
            int layoutType,
            BottomBarConfig.ButtonConfig buttonConfig,
            boolean addAsFirst) {
        ImageButton button =
                (ImageButton)
                        LayoutInflater.from(mContext)
                                .inflate(layoutType, parent, /* attachToRoot= */ false);
        if (addAsFirst) {
            parent.addView(button, /* index= */ 0);
        } else {
            parent.addView(button);
        }
        updateButton(button, buttonConfig, /* isFirstTimeShown= */ true);
    }

    private LinearLayout getEvenLayoutRoot() {
        return (LinearLayout)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.google_bottom_bar_even, /* root= */ null);
    }

    private LinearLayout getSpotlightLayoutRoot() {
        return (LinearLayout)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.google_bottom_bar_spotlight, /* root= */ null);
    }
}
