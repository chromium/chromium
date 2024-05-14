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
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonConfig;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;

/** Builds the GoogleBottomBar view. */
public class GoogleBottomBarViewCreator {
    private final Context mContext;
    private final BottomBarConfig mConfig;
    private final GoogleBottomBarActionsHandler mActionsHandler;
    private View mRootView;

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
    public View createGoogleBottomBarView() {
        mRootView =
                (mConfig.getSpotlightId() != null)
                        ? createGoogleBottomBarSpotlightLayoutView()
                        : createGoogleBottomBarEvenLayoutView();

        initSaveButton(mRootView);

        return mRootView;
    }

    boolean updateBottomBarButton(@Nullable ButtonConfig buttonConfig) {
        if (buttonConfig == null) {
            return false;
        }
        ImageView button = mRootView.findViewById(buttonConfig.getId());
        return maybeUpdateButton(button, buttonConfig);
    }

    private void initSaveButton(View rootView) {
        ImageView imageView = rootView.findViewById(R.id.google_bottom_bar_save_button);
        ButtonConfig buttonConfig = findButtonConfig(ButtonId.SAVE);
        maybeUpdateButton(imageView, buttonConfig);
    }

    private @Nullable ButtonConfig findButtonConfig(@ButtonId int buttonId) {
        return mConfig.getButtonList().stream()
                .filter(config -> config.getId() == buttonId)
                .findFirst()
                .orElse(null);
    }

    private View createGoogleBottomBarEvenLayoutView() {
        return LayoutInflater.from(mContext).inflate(R.layout.google_bottom_bar_even, null);
    }

    private View createGoogleBottomBarSpotlightLayoutView() {
        return LayoutInflater.from(mContext).inflate(R.layout.google_bottom_bar_spotlight, null);
    }

    private boolean maybeUpdateButton(
            @Nullable ImageView button, @Nullable ButtonConfig buttonConfig) {
        if (button == null || buttonConfig == null) {
            return false;
        }
        button.setId(buttonConfig.getId());
        button.setImageDrawable(buttonConfig.getIcon());
        button.setContentDescription(buttonConfig.getDescription());
        button.setOnClickListener(mActionsHandler.getClickListener(buttonConfig));
        return true;
    }
}
