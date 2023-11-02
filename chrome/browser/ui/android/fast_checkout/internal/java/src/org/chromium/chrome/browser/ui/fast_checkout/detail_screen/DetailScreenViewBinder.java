// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_LIST_HEIGHT_IN_PX;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_TITLE_DESCRIPTION;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * A ViewHolder and a static bind method for FastCheckout's Autofill profile screen.
 */
public class DetailScreenViewBinder {
    /** A ViewHolder that inflates the toolbar and provides easy item look-up. */
    static class ViewHolder {
        final Context mContext;
        final Toolbar mToolbar;
        final MenuItem mSettingsMenuItem;
        final FrameLayout mSheetItemListContainer;
        final RecyclerView mRecyclerView;

        ViewHolder(Context context, View contentView) {
            mContext = context;
            mSheetItemListContainer = contentView.findViewById(R.id.sheet_item_list_container);
            mRecyclerView =
                    contentView.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
            mToolbar = contentView.findViewById(R.id.action_bar);
            mSettingsMenuItem = mToolbar.getMenu().findItem(R.id.settings_menu_id);
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
            view.mToolbar.setNavigationOnClickListener(
                    (v) -> model.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run());
        } else if (propertyKey == DETAIL_SCREEN_SETTINGS_CLICK_HANDLER) {
            view.mToolbar.setOnMenuItemClickListener(
                    model.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER));
        } else if (propertyKey == DETAIL_SCREEN_TITLE) {
            view.mToolbar.setTitle(model.get(DETAIL_SCREEN_TITLE));
        } else if (propertyKey == DETAIL_SCREEN_TITLE_DESCRIPTION) {
            String text = view.mContext.getResources().getString(
                    model.get(DETAIL_SCREEN_TITLE_DESCRIPTION));
            view.mToolbar.setContentDescription(text);
        } else if (propertyKey == DETAIL_SCREEN_SETTINGS_MENU_TITLE) {
            view.mSettingsMenuItem.setTitle(model.get(DETAIL_SCREEN_SETTINGS_MENU_TITLE));
        } else if (propertyKey == DETAIL_SCREEN_MODEL_LIST) {
            SimpleRecyclerViewAdapter adapter =
                    new SimpleRecyclerViewAdapter(model.get(DETAIL_SCREEN_MODEL_LIST));
            adapter.registerType(DetailItemType.CREDIT_CARD, CreditCardItemViewBinder::create,
                    CreditCardItemViewBinder::bind);
            adapter.registerType(DetailItemType.FOOTER, FooterItemViewBinder::create,
                    FooterItemViewBinder::bind);
            adapter.registerType(DetailItemType.PROFILE, AutofillProfileItemViewBinder::create,
                    AutofillProfileItemViewBinder::bind);
            view.setAdapter(adapter);
        } else if (propertyKey == DETAIL_SCREEN_LIST_HEIGHT_IN_PX) {
            view.mSheetItemListContainer.getLayoutParams().height =
                    model.get(DETAIL_SCREEN_LIST_HEIGHT_IN_PX);
        }
    }
}
