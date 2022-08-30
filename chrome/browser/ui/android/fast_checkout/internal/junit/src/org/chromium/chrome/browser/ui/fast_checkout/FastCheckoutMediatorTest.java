// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.components.autofill_assistant.AutofillAssistantPublicTags;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller tests verify that the Fast Checkout controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FastCheckoutMediatorTest {
    private static final FastCheckoutAutofillProfile[] DUMMY_PROFILES = {
            FastCheckoutTestUtils.createDummyProfile("John Doe", "john@gmail.com"),
            FastCheckoutTestUtils.createDummyProfile("Jane Doe", "jane@gmail.com"),
            FastCheckoutTestUtils.createDummyProfile("Foo Boo", "foo@gmail.com")};
    private static final FastCheckoutCreditCard[] DUMMY_CARDS = {
            FastCheckoutTestUtils.createDummyCreditCard("https://example.com", "4111111111111111"),
            FastCheckoutTestUtils.createDummyCreditCard(
                    "https://example.co.uk", "4111111145454111")};

    @Mock
    RecyclerView mMockParentView;
    @Mock
    private FastCheckoutComponent.Delegate mMockDelegate;
    @Mock
    private BottomSheetContent mMockBottomSheetContent;
    @Mock
    private BottomSheetController mMockBottomSheetController;

    private FastCheckoutMediator mMediator = new FastCheckoutMediator();

    private final PropertyModel mModel = FastCheckoutProperties.createDefaultModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator.initialize(mMockDelegate, mModel, mMockBottomSheetController);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
        assertThat(mModel.get(PROFILE_MODEL_LIST), instanceOf(ListModel.class));
        assertThat(mModel.get(PROFILE_MODEL_LIST).size(), is(0));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST), instanceOf(ListModel.class));
        assertThat(mModel.get(CREDIT_CARD_MODEL_LIST).size(), is(0));

        // On top of that, the initialize method of the Mediator sets up delegates.
        assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        assertThat(
                mModel.get(HOME_SCREEN_DELEGATE), instanceOf(HomeScreenCoordinator.Delegate.class));

        assertNotNull(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER));
        assertThat(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER), instanceOf(Runnable.class));
    }

    @Test
    public void testSetCurrentScreenUpdatesModel() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);

        // Test that all relevant model entries got updated.
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));
        assertThat(mModel.get(DETAIL_SCREEN_TITLE),
                is(R.string.fast_checkout_autofill_profile_sheet_title));
        assertThat(mModel.get(DETAIL_SCREEN_SETTINGS_MENU_TITLE),
                is(R.string.fast_checkout_autofill_profile_settings_button_description));
        assertThat(mModel.get(DETAIL_SCREEN_MODEL_LIST), is(mModel.get(PROFILE_MODEL_LIST)));

        assertNotNull(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER));
        assertThat(mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER),
                instanceOf(OnMenuItemClickListener.class));
    }

    @Test
    public void testNavigateBackWorksFromAutofillProfileScreen() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));

        mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run();
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
    }

    @Test
    public void testOpenSettingsWorksFromAutofillProfileScreen() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILE_SCREEN));

        // Simulate the proper MenuItem.
        MenuItem settingsItem = mock(MenuItem.class);
        when(settingsItem.getItemId()).thenReturn(R.id.settings_menu_id);

        mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER).onMenuItemClick(settingsItem);
        verify(mMockDelegate).openAutofillProfileSettings();
    }

    @Test
    public void testOpenSettingsWorksFromCreditCardScreen() {
        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.CREDIT_CARD_SCREEN));

        // Simulate the proper MenuItem.
        MenuItem settingsItem = mock(MenuItem.class);
        when(settingsItem.getItemId()).thenReturn(R.id.settings_menu_id);

        mModel.get(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER).onMenuItemClick(settingsItem);
        verify(mMockDelegate).openCreditCardSettings();
    }

    @Test
    public void testShowOptionsSetsVisible() {
        mMediator.showOptions(DUMMY_PROFILES, DUMMY_CARDS);

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean());
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testAssistantOnboardingGetsHiddenIfShowing() {
        when(mMockParentView.getTag())
                .thenReturn(
                        AutofillAssistantPublicTags.AUTOFILL_ASSISTANT_BOTTOM_SHEET_CONTENT_TAG);
        when(mMockBottomSheetContent.getContentView()).thenReturn(mMockParentView);
        when(mMockBottomSheetController.getCurrentSheetContent())
                .thenReturn(mMockBottomSheetContent);

        mMediator.showOptions(DUMMY_PROFILES, DUMMY_CARDS);

        verify(mMockBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testSetAutofillProfilesCreatesModels() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.length));
        for (int index = 0; index < DUMMY_PROFILES.length; ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE),
                    is(DUMMY_PROFILES[index]));
        }
    }

    @Test
    public void testSetCreditCardsCreatesModels() {
        mMediator.setCreditCardItems(DUMMY_CARDS);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.length));
        for (int index = 0; index < DUMMY_CARDS.length; ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(CreditCardItemProperties.CREDIT_CARD), is(DUMMY_CARDS[index]));
        }
    }

    @Test
    public void testSetSelectedAutofillProfileUpdatesModels() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.length));

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[0]);
        checkThatAutofillProfileIsSelected(0);

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[2]);
        checkThatAutofillProfileIsSelected(2);
    }

    @Test
    public void testSetSelectedCreditCardUpdatesModels() {
        mMediator.setCreditCardItems(DUMMY_CARDS);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.length));

        mMediator.setSelectedCreditCard(DUMMY_CARDS[0]);
        checkThatCreditCardIsSelected(0);

        mMediator.setSelectedCreditCard(DUMMY_CARDS[1]);
        checkThatCreditCardIsSelected(1);
    }

    private void checkThatAutofillProfileIsSelected(int selectedIndex) {
        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        for (int index = 0; index < DUMMY_PROFILES.length; ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(AutofillProfileItemProperties.IS_SELECTED),
                    is(index == selectedIndex));
        }
    }

    private void checkThatCreditCardIsSelected(int selectedIndex) {
        ListModel<MVCListAdapter.ListItem> models = mModel.get(CREDIT_CARD_MODEL_LIST);
        for (int index = 0; index < DUMMY_CARDS.length; ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(CreditCardItemProperties.IS_SELECTED), is(index == selectedIndex));
        }
    }

    @Test
    public void testAutofillProfileItemOnClickListenerUpdatesSelectedProfile() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[0]);
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILE_SCREEN);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.length));

        PropertyModel model = models.get(2).model;
        model.get(AutofillProfileItemProperties.ON_CLICK_LISTENER).run();

        assertThat(mModel.get(SELECTED_PROFILE),
                is(model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE)));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
    }

    @Test
    public void testCreditCardItemOnClickListenerUpdatesSelectedCreditCard() {
        mMediator.setCreditCardItems(DUMMY_CARDS);
        mMediator.setSelectedCreditCard(DUMMY_CARDS[0]);
        mMediator.setCurrentScreen(ScreenType.CREDIT_CARD_SCREEN);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(CREDIT_CARD_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_CARDS.length));

        PropertyModel model = models.get(1).model;
        model.get(CreditCardItemProperties.ON_CLICK_LISTENER).run();

        assertThat(mModel.get(SELECTED_CREDIT_CARD),
                is(model.get(CreditCardItemProperties.CREDIT_CARD)));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
    }
}
