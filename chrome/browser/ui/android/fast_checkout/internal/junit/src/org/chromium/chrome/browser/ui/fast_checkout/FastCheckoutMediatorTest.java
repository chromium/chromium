// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CREDIT_CARD_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.PROFILE_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.SELECTED_CREDIT_CARD;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.SELECTED_PROFILE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.VISIBLE;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
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
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Controller tests verify that the Fast Checkout controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FastCheckoutMediatorTest {
    private static final List<FastCheckoutAutofillProfile> DUMMY_PROFILES =
            List.of(
                    FastCheckoutTestUtils.createDetailedProfile(
                            /* guid= */ "123",
                            /* name= */ "John Moe",
                            /* streetAddress= */ "Park Avenue 234",
                            /* city= */ "New York",
                            /* postalCode= */ "12345",
                            /* email= */ "john.moe@gmail.com",
                            /* phoneNumber= */ "+1-345-543-645"),
                    FastCheckoutTestUtils.createDetailedProfile(
                            /* guid= */ "234",
                            /* name= */ "Jane Doe",
                            /* streetAddress= */ "Sunset Blvd. 456",
                            /* city= */ "Los Angeles",
                            /* postalCode= */ "99999",
                            /* email= */ "doe.jane@gmail.com",
                            /* phoneNumber= */ "+1-345-333-319"),
                    FastCheckoutTestUtils.createDetailedProfile(
                            /* guid= */ "345",
                            /* name= */ "Foo Boo",
                            /* streetAddress= */ "Centennial Park",
                            /* city= */ "San Francisco",
                            /* postalCode= */ "23441",
                            /* email= */ "foo@gmail.com",
                            /* phoneNumber= */ "+1-205-333-009"));
    private static final List<FastCheckoutCreditCard> DUMMY_CARDS =
            List.of(
                    FastCheckoutTestUtils.createDummyCreditCard(
                            "xyz", "https://example.com", "4111111111111111"),
                    FastCheckoutTestUtils.createDummyCreditCard(
                            "hfg", "https://example.co.uk", "4111111145454111"),
                    FastCheckoutTestUtils.createDummyCreditCard(
                            "iyul", "https://neverseenbefore.com", "411167568911"),
                    FastCheckoutTestUtils.createDummyCreditCard(
                            "iyul", "https://www.example.com", "4118102027996045"));

    @Mock RecyclerView mMockParentView;
    @Mock private FastCheckoutComponent.Delegate mMockDelegate;
    @Mock private BottomSheetContent mMockBottomSheetContent;
    @Mock private BottomSheetController mMockBottomSheetController;

    private FastCheckoutMediator mMediator = new FastCheckoutMediator();
    private UserActionTester mActionTester;

    private final PropertyModel mModel = FastCheckoutProperties.createDefaultModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActionTester = new UserActionTester();
        mMediator.initialize(mMockDelegate, mModel, mMockBottomSheetController);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
        assertThat(mModel.get(PROFILE_MODEL_LIST), instanceOf(ModelList.class));
        assertThat(mModel.get(PROFILE_MODEL_LIST).size(), is(0));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST), instanceOf(ModelList.class));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(0));

        // On top of that, the initialize method of the Mediator sets up delegates.
        assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        assertThat(
                mModel.get(HOME_SCREEN_DELEGATE), instanceOf(HomeScreenCoordinator.Delegate.class));

        assertNotNull(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER));
        assertThat(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER), instanceOf(Runnable.class));
        assertActionRecorded(FastCheckoutUserActions.INITIALIZED);
    }

    @Test
    public void testSetCurrentScreenUpdatesModel() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));
        assertThat(
                mModel.get(DETAIL_SCREEN_TITLE),
                is(R.string.fast_checkout_autofill_profile_sheet_title));
        assertThat(
                mModel.get(DETAIL_SCREEN_SETTINGS_MENU_TITLE),
                is(R.string.fast_checkout_autofill_profile_settings_button_description));
        assertThat(mModel.get(DETAIL_SCREEN_MODEL_LIST), is(mModel.get(PROFILE_MODEL_LIST)));

        assertNotNull(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER));
        assertThat(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER), instanceOf(Runnable.class));

        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.CREDIT_CARD_SCREEN));
        assertThat(
                mModel.get(DETAIL_SCREEN_TITLE),
                is(R.string.fast_checkout_credit_card_sheet_title));
        assertThat(
                mModel.get(DETAIL_SCREEN_SETTINGS_MENU_TITLE),
                is(R.string.fast_checkout_credit_card_settings_button_description));
        assertThat(mModel.get(DETAIL_SCREEN_MODEL_LIST), is(mModel.get(CREDIT_CARD_MODEL_LIST)));

        assertNotNull(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER));
        assertThat(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER), instanceOf(Runnable.class));
    }

    @Test
    public void testNavigateBackWorksFromAutofillProfileScreen() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));

        mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run();
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
        assertActionRecorded(FastCheckoutUserActions.NAVIGATED_BACK_HOME);
    }

    @Test
    public void testOpenSettingsWorksFromAutofillProfileScreen() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));

        mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER).run();

        verify(mMockDelegate).openAutofillProfileSettings();
        assertActionRecorded(FastCheckoutUserActions.NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_ICON);
    }

    @Test
    public void testOpenSettingsWorksFromCreditCardScreen() {
        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.CREDIT_CARD_SCREEN));

        mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER).run();

        verify(mMockDelegate).openCreditCardSettings();
        assertActionRecorded(FastCheckoutUserActions.NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_ICON);
    }

    @Test
    public void testShowOptionsSetsVisible() {
        mMediator.showOptions(DUMMY_PROFILES, DUMMY_CARDS);

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean());
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testSetAutofillProfilesCreatesModels() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);

        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        // There is one extra item due to the footer.
        assertThat(models.size(), is(DUMMY_PROFILES.size() + 1));
        for (int index = 0; index < DUMMY_PROFILES.size(); ++index) {
            assertThat(models.get(index).type, is(DetailItemType.PROFILE));
            PropertyModel model = models.get(index).model;
            assertThat(
                    model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE),
                    is(DUMMY_PROFILES.get(index)));
        }
        assertThat(models.get(DUMMY_PROFILES.size()).type, is(DetailItemType.FOOTER));
    }

    @Test
    public void testSetCreditCardsCreatesModels() {
        mMediator.setCreditCardItems(DUMMY_CARDS);

        ModelList models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.size() + 1));
        for (int index = 0; index < DUMMY_CARDS.size(); ++index) {
            assertThat(models.get(index).type, is(DetailItemType.CREDIT_CARD));
            PropertyModel model = models.get(index).model;
            assertThat(model.get(CreditCardItemProperties.CREDIT_CARD), is(DUMMY_CARDS.get(index)));
        }
    }

    @Test
    public void testSetAutofillProfileItemsUpdatesPreservesPriorSelection() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        ModelList models = mModel.get(PROFILE_MODEL_LIST);

        // There is one extra item due to the footer.
        assertThat(models.size(), is(DUMMY_PROFILES.size() + 1));
        assertThat(mModel.get(SELECTED_PROFILE), is(DUMMY_PROFILES.get(0)));

        // If the old profile no longer exists, the new first one is selected.
        mMediator.setAutofillProfileItems(DUMMY_PROFILES.subList(1, 3));
        assertThat(mModel.get(PROFILE_MODEL_LIST).size(), is(3));
        assertThat(mModel.get(SELECTED_PROFILE), is(DUMMY_PROFILES.get(1)));

        // If it can be found, it remains selected.
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        assertThat(mModel.get(PROFILE_MODEL_LIST).size(), is(DUMMY_PROFILES.size() + 1));
        assertThat(mModel.get(SELECTED_PROFILE), is(DUMMY_PROFILES.get(1)));

        // That is true even if only the GUID remains the same.
        FastCheckoutAutofillProfile sameGUIDProfile =
                FastCheckoutTestUtils.createDetailedProfile(
                        DUMMY_PROFILES.get(1).getGUID(),
                        /* name= */ "Frank Tank",
                        /* streetAddress= */ "Somewhere 123",
                        /* city= */ "Des Moines",
                        /* postalCode= */ "93439",
                        /* email= */ "frank@tank.com",
                        /* phoneNumber= */ "+1-111-333-222");
        mMediator.setAutofillProfileItems(
                List.of(DUMMY_PROFILES.get(0), DUMMY_PROFILES.get(1), sameGUIDProfile));
        assertThat(mModel.get(PROFILE_MODEL_LIST).size(), is(4));
        assertThat(mModel.get(SELECTED_PROFILE), is(sameGUIDProfile));
    }

    @Test
    public void testSetCreditCardItemsUpdatesPreservesPriorSelection() {
        mMediator.setCreditCardItems(DUMMY_CARDS);

        // There is one extra item due to the footer.
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(DUMMY_CARDS.size() + 1));
        assertThat(mModel.get(SELECTED_CREDIT_CARD), is(DUMMY_CARDS.get(0)));

        // If the old profile no longer exists, the new first one is selected.
        mMediator.setCreditCardItems(DUMMY_CARDS.subList(1, 3));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(3));
        assertThat(mModel.get(SELECTED_CREDIT_CARD), is(DUMMY_CARDS.get(1)));

        // If it can be found, it remains selected.
        mMediator.setCreditCardItems(DUMMY_CARDS);
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(DUMMY_CARDS.size() + 1));
        assertThat(mModel.get(SELECTED_CREDIT_CARD), is(DUMMY_CARDS.get(1)));

        // That is true even if only the GUID remains the same.
        FastCheckoutCreditCard sameGUIDCard =
                FastCheckoutTestUtils.createDummyCreditCard(
                        DUMMY_CARDS.get(1).getGUID(), "https://example.fr", "56456551111");
        mMediator.setCreditCardItems(List.of(DUMMY_CARDS.get(0), DUMMY_CARDS.get(1), sameGUIDCard));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(4));
        assertThat(mModel.get(SELECTED_CREDIT_CARD), is(sameGUIDCard));
    }

    @Test
    public void testSetSelectedAutofillProfileUpdatesModels() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);

        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.size() + 1));

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES.get(0));
        checkThatAutofillProfileIsSelected(0);

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES.get(2));
        checkThatAutofillProfileIsSelected(2);
    }

    @Test
    public void testSetSelectedCreditCardUpdatesModels() {
        mMediator.setCreditCardItems(DUMMY_CARDS);

        ModelList models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.size() + 1));

        mMediator.setSelectedCreditCard(DUMMY_CARDS.get(0));
        checkThatCreditCardIsSelected(0);

        mMediator.setSelectedCreditCard(DUMMY_CARDS.get(1));
        checkThatCreditCardIsSelected(1);
    }

    private void checkThatAutofillProfileIsSelected(int selectedIndex) {
        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        for (int index = 0; index < DUMMY_PROFILES.size(); ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(
                    model.get(AutofillProfileItemProperties.IS_SELECTED),
                    is(index == selectedIndex));
        }
    }

    private void checkThatCreditCardIsSelected(int selectedIndex) {
        ModelList models = mModel.get(CREDIT_CARD_MODEL_LIST);
        for (int index = 0; index < DUMMY_CARDS.size(); ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(CreditCardItemProperties.IS_SELECTED), is(index == selectedIndex));
        }
    }

    @Test
    public void testAutofillProfileItemOnClickListenerUpdatesSelectedProfile() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES.get(0));
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);

        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.size() + 1));

        PropertyModel model = models.get(2).model;
        model.get(AutofillProfileItemProperties.ON_CLICK_LISTENER).run();

        assertThat(
                mModel.get(SELECTED_PROFILE),
                is(model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE)));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
        assertActionRecorded(FastCheckoutUserActions.SELECTED_DIFFERENT_ADDRESS);
    }

    @Test
    public void testCreditCardItemOnClickListenerUpdatesSelectedCreditCard() {
        mMediator.setCreditCardItems(DUMMY_CARDS);
        mMediator.setSelectedCreditCard(DUMMY_CARDS.get(0));
        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);

        ModelList models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.size() + 1));

        PropertyModel model = models.get(1).model;
        model.get(CreditCardItemProperties.ON_CLICK_LISTENER).run();

        assertThat(
                mModel.get(SELECTED_CREDIT_CARD),
                is(model.get(CreditCardItemProperties.CREDIT_CARD)));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
        assertActionRecorded(FastCheckoutUserActions.SELECTED_DIFFERENT_CREDIT_CARD);
    }

    @Test
    public void testFooterClickOnAutofillProfileSelectionOpensSettings() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES.get(0));
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);

        ModelList models = mModel.get(DETAIL_SCREEN_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.size() + 1));

        // Get the last item.
        PropertyModel model = models.get(DUMMY_PROFILES.size()).model;
        model.get(FooterItemProperties.ON_CLICK_HANDLER).run();
        verify(mMockDelegate).openAutofillProfileSettings();
        assertActionRecorded(FastCheckoutUserActions.NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_FOOTER);
    }

    @Test
    public void testFooterClickOnCreditCardSelectionOpensSettings() {
        mMediator.setCreditCardItems(DUMMY_CARDS);
        mMediator.setSelectedCreditCard(DUMMY_CARDS.get(0));
        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);

        ModelList models = mModel.get(DETAIL_SCREEN_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.size() + 1));

        // Get the last item.
        PropertyModel model = models.get(DUMMY_CARDS.size()).model;
        model.get(FooterItemProperties.ON_CLICK_HANDLER).run();
        verify(mMockDelegate).openCreditCardSettings();
        assertActionRecorded(FastCheckoutUserActions.NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_FOOTER);
    }

    @Test
    public void testHidesTheBottomSheetOnDestroy() {
        mMediator.showOptions(DUMMY_PROFILES, DUMMY_CARDS);

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean());
        assertThat(mModel.get(VISIBLE), is(true));

        mMediator.destroy();
        assertThat(mModel.get(VISIBLE), is(false));
        assertActionRecorded(FastCheckoutUserActions.DESTROYED);
    }

    private void assertActionRecorded(FastCheckoutUserActions action) {
        assertTrue(mActionTester.getActions().contains(action.getAction()));
    }
}
