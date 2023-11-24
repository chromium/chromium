// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** State for the Fast Checkout UI. */
public class FastCheckoutProperties {
    /** The different screens that can be shown on the sheet. */
    @IntDef({
        ScreenType.HOME_SCREEN,
        ScreenType.AUTOFILL_PROFILE_SCREEN,
        ScreenType.CREDIT_CARD_SCREEN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int HOME_SCREEN = 0;
        int AUTOFILL_PROFILE_SCREEN = 1;
        int CREDIT_CARD_SCREEN = 2;
    }

    /** The different item types in the RecyclerView on the Autofill profile sheet. */
    @IntDef({DetailItemType.PROFILE, DetailItemType.CREDIT_CARD, DetailItemType.FOOTER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DetailItemType {
        /** An Autofill profile entry. */
        int PROFILE = 1;

        /** A credit card entry. */
        int CREDIT_CARD = 2;

        /** A footer entry with a link to add a new profile/card. */
        int FOOTER = 3;
    }

    /** Property that indicates the bottom sheet visibility. */
    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    /**
     * Property that indicates which screen (i.e ScreenType) is currently displayed on the bottom
     * sheet.
     */
    public static final WritableIntPropertyKey CURRENT_SCREEN =
            new WritableIntPropertyKey("current_screen");

    /** The chosen autofill profile option. */
    public static final WritableObjectPropertyKey<FastCheckoutAutofillProfile> SELECTED_PROFILE =
            new WritableObjectPropertyKey<>("selected_profile");

    /** The models corresponding to all autofill profile options. */
    public static final WritableObjectPropertyKey<ModelList> PROFILE_MODEL_LIST =
            new WritableObjectPropertyKey("profile_model_list");

    /** The chosen credit card option. */
    public static final WritableObjectPropertyKey<FastCheckoutCreditCard> SELECTED_CREDIT_CARD =
            new WritableObjectPropertyKey<>("selected_credit_card");

    /** The models corresponding to all credit card options. */
    public static final WritableObjectPropertyKey<ModelList> CREDIT_CARD_MODEL_LIST =
            new WritableObjectPropertyKey("credit_card_model_list");

    /** The delegate that handles actions on the home screen. */
    public static final WritableObjectPropertyKey<HomeScreenCoordinator.Delegate>
            HOME_SCREEN_DELEGATE = new WritableObjectPropertyKey<>("home_screen_delegate");

    /** The string id of the title shown on the detail screen. */
    public static final WritableIntPropertyKey DETAIL_SCREEN_TITLE =
            new WritableIntPropertyKey("detail_screen_title");

    /** The string id of the accessibility description for the title shown on the detail screen. */
    public static final WritableIntPropertyKey DETAIL_SCREEN_TITLE_DESCRIPTION =
            new WritableIntPropertyKey("detail_screen_title");

    /** The string id of the title shown on the settings icon. */
    public static final WritableIntPropertyKey DETAIL_SCREEN_SETTINGS_MENU_TITLE =
            new WritableIntPropertyKey("detail_screen_settings_menu_title");

    /** The handler for the back icon on the detail screens (autofill profiles, credit cards). */
    public static final WritableObjectPropertyKey<Runnable> DETAIL_SCREEN_BACK_CLICK_HANDLER =
            new WritableObjectPropertyKey<>("detail_screen_back_click_handler");

    /** The handler for the settings icon on the autofill profile screen. */
    public static final WritableObjectPropertyKey<Runnable> DETAIL_SCREEN_SETTINGS_CLICK_HANDLER =
            new WritableObjectPropertyKey<>("detail_screen_settings_click_handler");

    /**
     * The models that are displayed on the detail screen. This will either point to
     * PROFILE_MODEL_LIST or CREDIT_CARD_MODEL_LIST.
     */
    public static final WritableObjectPropertyKey<ModelList> DETAIL_SCREEN_MODEL_LIST =
            new WritableObjectPropertyKey<>("detail_screen_model_list");

    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenType.HOME_SCREEN)
                .with(PROFILE_MODEL_LIST, new ModelList())
                .with(CREDIT_CARD_MODEL_LIST, new ModelList())
                .build();
    }

    /** All keys used for the fast checkout bottom sheet. */
    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                VISIBLE,
                CURRENT_SCREEN,
                SELECTED_PROFILE,
                PROFILE_MODEL_LIST,
                SELECTED_CREDIT_CARD,
                CREDIT_CARD_MODEL_LIST,
                HOME_SCREEN_DELEGATE,
                DETAIL_SCREEN_TITLE,
                DETAIL_SCREEN_TITLE_DESCRIPTION,
                DETAIL_SCREEN_SETTINGS_MENU_TITLE,
                DETAIL_SCREEN_BACK_CLICK_HANDLER,
                DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                DETAIL_SCREEN_MODEL_LIST
            };
}
