// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.DOUBLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.NO_VARIANT;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.GoogleBottomBarVariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_HOME;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_LENS;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarVariantCreatedEvent;
import org.chromium.ui.base.ViewUtils;

/** Builds the GoogleBottomBar view. */
public class GoogleBottomBarViewCreator {
    private static final String TAG = "GbbViewCreator";

    private static final int SINGLE_DECKER_MIN_HEIGHT_DP_FOR_LARGE_TOP_PADDING = 60;

    private final Context mContext;
    private final BottomBarConfig mConfig;
    private final GoogleBottomBarActionsHandler mActionsHandler;
    private ViewGroup mRootView;

    /**
     * Constructor.
     *
     * @param activity An Android activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param shareDelegateSupplier Supplier for the the share delegate.
     * @param config Bottom bar configuration for the buttons that will be displayed.
     */
    public GoogleBottomBarViewCreator(
            Activity activity,
            Supplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            BottomBarConfig config) {
        mContext = activity;
        mConfig = config;
        mActionsHandler =
                new GoogleBottomBarActionsHandler(activity, tabProvider, shareDelegateSupplier);
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
    View createGoogleBottomBarView() {
        mRootView = getLayoutRoot();

        setDimensionsOnRootView();
        maybeAddButtons();
        maybeAddSearchbox();
        maybeAddButtonsOnRight();

        return mRootView;
    }

    /** Calculates and returns the height of the bottom bar in pixels. */
    int getBottomBarHeightInPx() {
        return ViewUtils.dpToPx(mContext, (float) mConfig.getHeightDp());
    }

    private void maybeAddSearchbox() {
        ViewGroup searchboxContainer = mRootView.findViewById(R.id.bottom_bar_searchbox_container);
        if (searchboxContainer != null) {
            searchboxContainer.addView(
                    createGoogleBottomBarSearchboxLayoutView(searchboxContainer));
        }
    }

    private View createGoogleBottomBarSearchboxLayoutView(ViewGroup searchboxContainer) {
        View searchboxView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.google_bottom_bar_searchbox,
                                searchboxContainer,
                                /* attachToRoot= */ false);

        searchboxView
                .findViewById(R.id.google_bottom_bar_searchbox_super_g_button)
                .setOnClickListener(v -> mActionsHandler.onSearchboxHomeTap());
        GoogleBottomBarLogger.logButtonShown(SEARCHBOX_HOME);

        searchboxView
                .findViewById(R.id.google_bottom_bar_searchbox_hint_text_view)
                .setOnClickListener(v -> mActionsHandler.onSearchboxHintTextTap());
        GoogleBottomBarLogger.logButtonShown(SEARCHBOX_SEARCH);

        searchboxView
                .findViewById(R.id.google_bottom_bar_searchbox_mic_button)
                .setOnClickListener(v -> mActionsHandler.onSearchboxMicTap());
        GoogleBottomBarLogger.logButtonShown(SEARCHBOX_VOICE_SEARCH);

        searchboxView
                .findViewById(R.id.google_bottom_bar_searchbox_lens_button)
                .setOnClickListener(v -> mActionsHandler.onSearchboxLensTap(v));
        GoogleBottomBarLogger.logButtonShown(SEARCHBOX_LENS);

        return searchboxView;
    }

    private void maybeAddButtons() {
        ViewGroup buttonsContainer = mRootView.findViewById(R.id.bottom_bar_buttons_container);
        if (buttonsContainer != null) {
            buttonsContainer.addView(
                    (mConfig.getSpotlightId() != null)
                            ? createGoogleBottomBarSpotlightLayoutView()
                            : createGoogleBottomBarEvenLayoutView());
        }
    }

    private void maybeAddButtonsOnRight() {
        LinearLayout buttonsOnRightContainer =
                mRootView.findViewById(R.id.bottom_bar_buttons_on_right_container);
        if (buttonsOnRightContainer != null) {
            for (BottomBarConfig.ButtonConfig buttonConfig : mConfig.getButtonList()) {
                createButtonAndAddToParent(
                        buttonsOnRightContainer,
                        R.layout.bottom_bar_button_non_spotlight_layout,
                        buttonConfig,
                        /* addAsFirst= */ false);
            }
        }
    }

    private ViewGroup getLayoutRoot() {
        int rootLayoutId = 0;
        switch (mConfig.getVariantLayoutType()) {
            case NO_VARIANT -> {
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.NO_VARIANT);
                rootLayoutId = R.layout.bottom_bar_no_variant_root_layout;
            }
            case DOUBLE_DECKER -> {
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.DOUBLE_DECKER);
                rootLayoutId = R.layout.bottom_bar_double_decker_root_layout;
            }
            case SINGLE_DECKER -> {
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER);
                rootLayoutId = R.layout.bottom_bar_single_decker_root_layout;
            }
            case SINGLE_DECKER_WITH_RIGHT_BUTTONS -> {
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER_WITH_RIGHT_BUTTONS);
                rootLayoutId = R.layout.bottom_bar_single_decker_with_right_buttons_root_layout;
            }
            default -> {
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.UNKNOWN_VARIANT);
                GoogleBottomBarLogger.logVariantCreatedEvent(
                        GoogleBottomBarVariantCreatedEvent.NO_VARIANT);
                Log.e(
                        TAG,
                        "Unexpected variant layout type: %s. Fallback to no variant" + " layout.",
                        mConfig.getVariantLayoutType());
                rootLayoutId = R.layout.bottom_bar_no_variant_root_layout;
            }
        }
        return (ViewGroup) LayoutInflater.from(mContext).inflate(rootLayoutId, /* root= */ null);
    }

    private void setDimensionsOnRootView() {
        mRootView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, getBottomBarHeightInPx()));

        if (mConfig.getVariantLayoutType() == SINGLE_DECKER
                || mConfig.getVariantLayoutType() == SINGLE_DECKER_WITH_RIGHT_BUTTONS) {
            // We prefer large (8dp) top padding, but if this will cause searchbox to be below 52dp
            // we reduce it to small (4dp).
            int singleDeckerTopPaddingResId =
                    mConfig.getHeightDp() >= SINGLE_DECKER_MIN_HEIGHT_DP_FOR_LARGE_TOP_PADDING
                            ? R.dimen.google_bottom_bar_single_decker_top_padding_large
                            : R.dimen.google_bottom_bar_single_decker_top_padding_small;
            mRootView.setPaddingRelative(
                    /* start= */ mRootView.getPaddingStart(),
                    /* top= */ mContext.getResources()
                            .getDimensionPixelSize(singleDeckerTopPaddingResId),
                    /* end= */ mRootView.getPaddingEnd(),
                    /* bottom= */ mRootView.getPaddingBottom());
        }
    }

    boolean updateBottomBarButton(@Nullable ButtonConfig buttonConfig) {
        if (buttonConfig == null) {
            return false;
        }
        ImageButton button = mRootView.findViewById(buttonConfig.getId());
        return button != null && updateButton(button, buttonConfig, /* isFirstTimeShown= */ false);
    }

    void logButtons() {
        for (ButtonConfig buttonConfig : mConfig.getButtonList()) {
            View button = mRootView.findViewById(buttonConfig.getId());
            if (button != null) {
                int buttonEvent = GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(buttonConfig);
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

        LinearLayout rootView = getEvenButtonsLayout();
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

        LinearLayout rootView = getSpotlightButtonsLayout();

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
            int buttonEvent = GoogleBottomBarLogger.getGoogleBottomBarButtonEvent(buttonConfig);
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

    private LinearLayout getEvenButtonsLayout() {
        return (LinearLayout)
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.google_bottom_bar_even,
                                mRootView,
                                /* attachToRoot= */ false);
    }

    private LinearLayout getSpotlightButtonsLayout() {
        return (LinearLayout)
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.google_bottom_bar_spotlight,
                                mRootView,
                                /* attachToRoot= */ false);
    }
}
