// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.FooterItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the FastCheckout component. It sets the state of the model and reacts to
 * events like clicks.
 */
public class FastCheckoutMediator implements FastCheckoutSheetState {
    private PropertyModel mModel;
    private FastCheckoutComponent.Delegate mDelegate;
    private BottomSheetController mBottomSheetController;
    private BottomSheetObserver mBottomSheetDismissedObserver;

    void initialize(
            FastCheckoutComponent.Delegate delegate,
            PropertyModel model,
            BottomSheetController bottomSheetController) {
        mModel = model;
        mDelegate = delegate;
        mBottomSheetController = bottomSheetController;

        mBottomSheetDismissedObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                        super.onSheetClosed(reason);
                        dismiss(reason);
                        mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
                    }
                };

        mModel.set(FastCheckoutProperties.HOME_SCREEN_DELEGATE, createHomeScreenDelegate());
        mModel.set(
                FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> {
                    setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN);
                    FastCheckoutUserActions.NAVIGATED_BACK_HOME.log();
                });
        FastCheckoutUserActions.INITIALIZED.log();
    }

    /** Returns an implementation the {@link HomeScreenCoordinator.Delegate} interface. */
    private HomeScreenCoordinator.Delegate createHomeScreenDelegate() {
        return new HomeScreenCoordinator.Delegate() {
            @Override
            public void onOptionsAccepted() {
                if (!mModel.get(FastCheckoutProperties.VISIBLE)) {
                    return; // Dismiss only if not dismissed yet.
                }
                FastCheckoutUserActions.ACCEPTED.log();
                FastCheckoutAutofillProfile profile =
                        mModel.get(FastCheckoutProperties.SELECTED_PROFILE);
                FastCheckoutCreditCard creditCard =
                        mModel.get(FastCheckoutProperties.SELECTED_CREDIT_CARD);
                assert profile != null && creditCard != null;
                mModel.set(FastCheckoutProperties.VISIBLE, false);
                mDelegate.onOptionsSelected(profile, creditCard);
            }

            @Override
            public void onShowAddressesList() {
                setCurrentScreen(FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN);
                FastCheckoutUserActions.NAVIGATED_TO_ADDRESSES.log();
            }

            @Override
            public void onShowCreditCardList() {
                setCurrentScreen(FastCheckoutProperties.ScreenType.CREDIT_CARD_SCREEN);
                FastCheckoutUserActions.NAVIGATED_TO_CREDIT_CARDS.log();
            }
        };
    }

    public void showOptions(
            List<FastCheckoutAutofillProfile> profiles, List<FastCheckoutCreditCard> creditCards) {
        setAutofillProfileItems(profiles);
        setCreditCardItems(creditCards);
        setCurrentScreen(mModel.get(FastCheckoutProperties.CURRENT_SCREEN));
        // Show the bottom sheet.
        mModel.set(FastCheckoutProperties.VISIBLE, true);
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @param content The bottom sheet content to show/hide.
     * @return True if the request was successful, false otherwise.
     */
    public boolean setVisible(boolean isVisible, BottomSheetContent content) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetDismissedObserver);
            if (!mBottomSheetController.requestShowContent(content, true)) {
                mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(content, true);
        }
        return true;
    }

    /** Dismisses the current bottom sheet. */
    public void dismiss(@StateChangeReason int reason) {
        if (!mModel.get(FastCheckoutProperties.VISIBLE)) {
            return; // Dismiss only if not dismissed yet.
        }
        FastCheckoutUserActions.DISMISSED.log();
        mModel.set(FastCheckoutProperties.VISIBLE, false);
        mDelegate.onDismissed();
    }

    /**
     * Sets the Autofill profile items and creates the corresponding models for the profile item
     * entries on the Autofill profiles page. If there is a selected Autofill profile prior to
     * calling this method, the profile with the same GUID will remain selected. If no prior
     * selection was made or this GUID no longer exists, the first Autofill profile is selected.
     *
     * @param profiles The array of FastCheckoutAutofillProfile to set as Autofill profiles.
     */
    public void setAutofillProfileItems(List<FastCheckoutAutofillProfile> profiles) {
        assert profiles != null && profiles.size() != 0;

        FastCheckoutAutofillProfile previousSelection =
                mModel.get(FastCheckoutProperties.SELECTED_PROFILE);
        FastCheckoutAutofillProfile newSelection = profiles.get(0);

        // Populate all model entries.
        ModelList profileItems = new ModelList();
        for (FastCheckoutAutofillProfile profile : profiles) {
            if (previousSelection != null
                    && profile.getGUID().equals(previousSelection.getGUID())) {
                newSelection = profile;
            }
            PropertyModel model =
                    AutofillProfileItemProperties.create(
                            /* profile= */ profile,
                            /* isSelected= */ false,
                            /* onClickListener= */ () -> {
                                setSelectedAutofillProfile(profile);
                                setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN);
                            });
            profileItems.add(new ListItem(DetailItemType.PROFILE, model));
        }

        // Add the footer item.
        profileItems.add(
                new ListItem(
                        DetailItemType.FOOTER,
                        FooterItemProperties.create(
                                /* label= */ R.string
                                        .fast_checkout_detail_screen_add_autofill_profile_text,
                                /* onClickHandler= */ () -> {
                                    mDelegate.openAutofillProfileSettings();
                                    FastCheckoutUserActions
                                            .NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_FOOTER
                                            .log();
                                })));

        mModel.set(FastCheckoutProperties.PROFILE_MODEL_LIST, profileItems);
        setSelectedAutofillProfile(newSelection);
    }

    /**
     * Sets the selected Autofill profile and updates the IS_SELECTED entry in the models
     * of the profile item entries on the Autofill profiles page.
     * @param selectedProfile The profile that is to be selected.
     */
    public void setSelectedAutofillProfile(FastCheckoutAutofillProfile selectedProfile) {
        assert selectedProfile != null;
        boolean isInitialSelection = mModel.get(FastCheckoutProperties.SELECTED_PROFILE) == null;
        mModel.set(FastCheckoutProperties.SELECTED_PROFILE, selectedProfile);

        int foundProfiles = 0;
        ModelList allItems = mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST);
        for (ListItem item : allItems) {
            if (item.type != DetailItemType.PROFILE) {
                continue;
            }
            boolean isSelected =
                    selectedProfile.equals(
                            item.model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE));
            boolean wasSelected = item.model.get(AutofillProfileItemProperties.IS_SELECTED);
            item.model.set(AutofillProfileItemProperties.IS_SELECTED, isSelected);
            if (isSelected) {
                ++foundProfiles;
                if (!isInitialSelection) {
                    if (wasSelected) {
                        FastCheckoutUserActions.SELECTED_SAME_ADDRESS.log();
                    } else {
                        FastCheckoutUserActions.SELECTED_DIFFERENT_ADDRESS.log();
                    }
                }
            }
        }

        // Exactly one of the models must contain the selected profile.
        assert foundProfiles == 1;
    }

    /**
     * Sets the credit card items and creates the corresponding models for the credit card item
     * entries on the credit card page. If there is a selected credit card prior to calling this
     * method, the card with the same GUID will remain selected. If no prior selection was made or
     * this GUID no longer exists, the first credit card is selected.
     *
     * @param creditCards The array of FastCheckoutCreditCard to set as credit cards.
     */
    public void setCreditCardItems(List<FastCheckoutCreditCard> creditCards) {
        assert creditCards != null && creditCards.size() != 0;

        FastCheckoutCreditCard previousSelection =
                mModel.get(FastCheckoutProperties.SELECTED_CREDIT_CARD);
        FastCheckoutCreditCard newSelection = creditCards.get(0);

        // Populate all model entries.
        ModelList cardItems = new ModelList();

        for (FastCheckoutCreditCard card : creditCards) {
            if (previousSelection != null && card.getGUID().equals(previousSelection.getGUID())) {
                newSelection = card;
            }
            PropertyModel model =
                    CreditCardItemProperties.create(
                            /* creditCard= */ card,
                            /* isSelected= */ false,
                            /* onClickListener= */ () -> {
                                setSelectedCreditCard(card);
                                setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN);
                            });
            ListItem item = new ListItem(DetailItemType.CREDIT_CARD, model);
            cardItems.add(item);
        }

        // Add the footer item.
        cardItems.add(
                new ListItem(
                        DetailItemType.FOOTER,
                        FooterItemProperties.create(
                                /* label= */ R.string
                                        .fast_checkout_detail_screen_add_credit_card_text,
                                /* onClickHandler= */ () -> {
                                    mDelegate.openCreditCardSettings();
                                    FastCheckoutUserActions
                                            .NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_FOOTER
                                            .log();
                                })));

        mModel.set(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST, cardItems);
        setSelectedCreditCard(newSelection);
    }

    /**
     * Sets the selected credit card and updates the IS_SELECTED entry in the models
     * of the credit card item entries on the credit card page.
     * @param selectedCreditCard The credit card that is to be selected.
     */
    public void setSelectedCreditCard(FastCheckoutCreditCard selectedCreditCard) {
        assert selectedCreditCard != null;
        boolean isInitialSelection =
                mModel.get(FastCheckoutProperties.SELECTED_CREDIT_CARD) == null;
        mModel.set(FastCheckoutProperties.SELECTED_CREDIT_CARD, selectedCreditCard);

        int foundCards = 0;
        ModelList allItems = mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST);
        for (ListItem item : allItems) {
            if (item.type != DetailItemType.CREDIT_CARD) {
                continue;
            }
            boolean isSelected =
                    selectedCreditCard.equals(item.model.get(CreditCardItemProperties.CREDIT_CARD));
            boolean wasSelected = item.model.get(CreditCardItemProperties.IS_SELECTED);
            item.model.set(CreditCardItemProperties.IS_SELECTED, isSelected);
            if (isSelected) {
                ++foundCards;
                if (!isInitialSelection) {
                    if (wasSelected) {
                        FastCheckoutUserActions.SELECTED_SAME_CREDIT_CARD.log();
                    } else {
                        FastCheckoutUserActions.SELECTED_DIFFERENT_CREDIT_CARD.log();
                    }
                }
            }
        }

        // Exactly one of the models must contain the selected credit card.
        assert foundCards == 1;
    }

    /**
     * Selects the currently shown screen on the bottomsheet.
     * @param screenType A {@link FastCheckoutProperties.ScreenType} that defines the screen to be
     *         shown.
     */
    public void setCurrentScreen(int screenType) {
        if (screenType == FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN) {
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_TITLE,
                    R.string.fast_checkout_autofill_profile_sheet_title);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_TITLE_DESCRIPTION,
                    R.string.fast_checkout_autofill_profile_sheet_title_description);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE,
                    R.string.fast_checkout_autofill_profile_settings_button_description);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    () -> {
                        mDelegate.openAutofillProfileSettings();
                        FastCheckoutUserActions.NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_ICON.log();
                    });
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST));
        } else if (screenType == ScreenType.CREDIT_CARD_SCREEN) {
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_TITLE,
                    R.string.fast_checkout_credit_card_sheet_title);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_TITLE_DESCRIPTION,
                    R.string.fast_checkout_credit_card_sheet_title_description);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE,
                    R.string.fast_checkout_credit_card_settings_button_description);
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    () -> {
                        mDelegate.openCreditCardSettings();
                        FastCheckoutUserActions.NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_ICON.log();
                    });
            mModel.set(
                    FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST));
        }

        mModel.set(FastCheckoutProperties.CURRENT_SCREEN, screenType);
        // Sets bottom sheet to half height, if enabled. Otherwise to full.
        mBottomSheetController.expandSheet();
    }

    /** Releases the resources used by FastCheckoutMediator. */
    @MainThread
    public void destroy() {
        FastCheckoutUserActions.DESTROYED.log();
        mModel.set(FastCheckoutProperties.VISIBLE, false);
    }

    @Override
    public @ScreenType int getCurrentScreen() {
        return mModel.get(FastCheckoutProperties.CURRENT_SCREEN);
    }

    @Override
    public int getNumOfAutofillProfiles() {
        // The list contains Autofill profiles and one footer at the end. Subtracts 1 to not count
        // the footer.
        return mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST).size() - 1;
    }

    @Override
    public int getNumOfCreditCards() {
        // The list contains credit cards and one footer at the end. Subtracts 1 to not count the
        // footer.
        return mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST).size() - 1;
    }

    @Override
    public int getContainerHeight() {
        return mBottomSheetController.getContainerHeight();
    }
}
