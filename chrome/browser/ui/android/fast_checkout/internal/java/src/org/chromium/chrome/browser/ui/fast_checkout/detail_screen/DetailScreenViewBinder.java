// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_TITLE_DESCRIPTION;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.HOME_SCREEN;

import android.content.Context;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.bottom_sheet_utils.DetailScreenScrollListener;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** A ViewHolder and a static bind method for FastCheckout's Autofill profile screen. */
public class DetailScreenViewBinder {
    /** A ViewHolder that inflates the toolbar and provides easy item look-up. */
    static class ViewHolder {
        final Context mContext;
        final ImageButton mToolbarBackImageButton;
        final TextView mToolbarTitleTextView;
        final ImageButton mToolbarSettingsImageButton;
        final View mToolbarA11yOverlayView;
        final RecyclerView mRecyclerView;
        final DetailScreenScrollListener mScrollListener;

        ViewHolder(Context context, View contentView, DetailScreenScrollListener scrollListener) {
            mContext = context;
            mRecyclerView =
                    contentView.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
            mToolbarBackImageButton =
                    contentView.findViewById(R.id.fast_checkout_toolbar_back_image_button);
            mToolbarTitleTextView =
                    contentView.findViewById(R.id.fast_checkout_toolbar_title_text_view);
            mToolbarSettingsImageButton =
                    contentView.findViewById(R.id.fast_checkout_toolbar_settings_image_button);
            mToolbarA11yOverlayView =
                    contentView.findViewById(R.id.fast_checkout_toolbar_a11y_overlay_view);

            mScrollListener = scrollListener;
        }

        /** Sets the adapter for the RecyclerView that contains the profile items. */
        public void setAdapter(RecyclerView.Adapter adapter) {
            mRecyclerView.setAdapter(adapter);
        }
    }

    /**
     * Binds the {@link ViewHolder} to a {@link PropertyModel} with properties defined in
     * {@link FastCheckoutProperties} and prefix DETAIL_SCREEN.
     */
    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == DETAIL_SCREEN_BACK_CLICK_HANDLER) {
            view.mToolbarBackImageButton.setOnClickListener(
                    (v) -> model.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run());
        } else if (propertyKey == DETAIL_SCREEN_SETTINGS_CLICK_HANDLER) {
            view.mToolbarSettingsImageButton.setOnClickListener(
                    (v) -> model.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER).run());
        } else if (propertyKey == DETAIL_SCREEN_TITLE) {
            String titleText =
                    view.mContext.getResources().getString(model.get(DETAIL_SCREEN_TITLE));
            view.mToolbarTitleTextView.setText(titleText);
        } else if (propertyKey == DETAIL_SCREEN_TITLE_DESCRIPTION) {
            String titleContentDescription =
                    view.mContext
                            .getResources()
                            .getString(model.get(DETAIL_SCREEN_TITLE_DESCRIPTION));
            view.mToolbarA11yOverlayView.setContentDescription(titleContentDescription);
        } else if (propertyKey == DETAIL_SCREEN_SETTINGS_MENU_TITLE) {
            String settingsContentDescription =
                    view.mContext
                            .getResources()
                            .getString(model.get(DETAIL_SCREEN_SETTINGS_MENU_TITLE));
            view.mToolbarSettingsImageButton.setContentDescription(settingsContentDescription);
        } else if (propertyKey == DETAIL_SCREEN_MODEL_LIST) {
            SimpleRecyclerViewAdapter adapter =
                    new SimpleRecyclerViewAdapter(model.get(DETAIL_SCREEN_MODEL_LIST));
            adapter.registerType(
                    DetailItemType.CREDIT_CARD,
                    CreditCardItemViewBinder::create,
                    CreditCardItemViewBinder::bind);
            adapter.registerType(
                    DetailItemType.FOOTER,
                    FooterItemViewBinder::create,
                    FooterItemViewBinder::bind);
            adapter.registerType(
                    DetailItemType.PROFILE,
                    AutofillProfileItemViewBinder::create,
                    AutofillProfileItemViewBinder::bind);
            view.setAdapter(adapter);
        } else if (propertyKey == CURRENT_SCREEN && model.get(CURRENT_SCREEN) == HOME_SCREEN) {
            view.mRecyclerView.suppressLayout(/* suppress= */ false);
            ((LinearLayoutManager) view.mRecyclerView.getLayoutManager())
                    .scrollToPositionWithOffset(/* position= */ 0, /* offset= */ 0);
            view.mScrollListener.reset();
        }
    }
}
