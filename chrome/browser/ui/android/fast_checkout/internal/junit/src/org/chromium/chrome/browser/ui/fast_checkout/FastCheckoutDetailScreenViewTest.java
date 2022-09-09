// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CREDIT_CARD_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.PROFILE_MODEL_LIST;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.DetailScreenCoordinator;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.FooterItemProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Simple unit tests for the detail screen view.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
public class FastCheckoutDetailScreenViewTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private Runnable mBackClickHandler;
    @Mock
    private Runnable mSettingsClickHandler;

    private View mView;
    private PropertyModel mModel;
    private static final FastCheckoutAutofillProfile sSampleProfile1 =
            FastCheckoutTestUtils.createDetailedProfile(
                    /*guid=*/"111", /*name=*/"John Moe", /*streetAddress=*/"Park Avenue 234",
                    /*city=*/"New York", /*postalCode=*/"12345", /*email=*/"john.moe@gmail.com",
                    /*phoneNumber=*/"+1-345-543-645");
    private static final FastCheckoutAutofillProfile sSampleProfile2 =
            FastCheckoutTestUtils.createDetailedProfile(
                    /*guid=*/"555", /*name=*/"Jane Doe", /*streetAddress=*/"Sunset Blvd. 456",
                    /*city=*/"Los Angeles", /*postalCode=*/"99999", /*email=*/"doe.jane@gmail.com",
                    /*phoneNumber=*/"+1-345-333-319");

    private static final FastCheckoutCreditCard sSampleCard1 =
            FastCheckoutTestUtils.createDetailedCreditCard(/*guid=*/"123",
                    /*origin=*/"https://example.com", /*name=*/"John Moe", /*number=*/"75675675656",
                    /*obfuscatedNumber=*/"5656", /*month=*/"05", /*year=*/"2031",
                    /*issuerIconString=*/"visaCC");

    private static final FastCheckoutCreditCard sSampleCard2 =
            FastCheckoutTestUtils.createDetailedCreditCard(/*guid=*/"154",
                    /*origin=*/"https://example.fr", /*name=*/"Jane Doe",
                    /*number=*/"4564565541234",
                    /*obfuscatedNumber*/ "1234", /*month=*/"10", /*year=*/"2025",
                    /*issuerIconString=*/"dinersCC");

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> {
            mModel = FastCheckoutProperties.createDefaultModel();
            mModel.set(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    FastCheckoutMediator.createSettingsOnClickListener(mSettingsClickHandler));
            mModel.set(DETAIL_SCREEN_BACK_CLICK_HANDLER, mBackClickHandler);

            // Create the view.
            mView = LayoutInflater.from(activity).inflate(
                    R.layout.fast_checkout_detail_screen_sheet, null);

            // Let the coordinator connect model and view.
            new DetailScreenCoordinator(activity, mView, mModel);
            activity.setContentView(mView);
            assertNotNull(mView);
        });
    }

    @Test
    @SmallTest
    public void testBackArrowClickCallsHandler() {
        assertNotNull(mView);
        Toolbar toolbar = mView.findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        // Find the navigation button. Toolbar does not expose a method to get
        // the navigation button and Espresso does not work in this setup.
        // TODO(crbug.com/1355310): Move to integration test once that exists.
        View backButton = null;
        for (int index = 0; index < toolbar.getChildCount(); ++index) {
            View candidateView = toolbar.getChildAt(index);
            if (candidateView.getContentDescription() != null
                    && candidateView.getContentDescription().equals(
                            toolbar.getNavigationContentDescription())) {
                backButton = candidateView;
            }
        }
        assertNotNull(backButton);
        backButton.performClick();

        ShadowLooper.shadowMainLooper().idle();
        verify(mBackClickHandler).run();
    }

    @Test
    @SmallTest
    public void testOpenSettingsClickCallsHandler() {
        // Click on the settings element.
        View settingsMenuElement = mView.findViewById(R.id.settings_menu_id);
        assertNotNull(settingsMenuElement);
        settingsMenuElement.performClick();

        ShadowLooper.shadowMainLooper().idle();
        verify(mSettingsClickHandler).run();
    }

    @Test
    @SmallTest
    public void testRecyclerViewPopulatesProfileItemEntriesAndReactsToClicks() {
        Runnable callback1 = mock(Runnable.class);
        Runnable callback2 = mock(Runnable.class);
        ModelList models = mModel.get(PROFILE_MODEL_LIST);

        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(sSampleProfile1, /*isSelected=*/false,
                        /*onClickListener=*/callback1)));
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(sSampleProfile2, /*isSelected=*/true,
                        /*onClickListener=*/callback2)));

        mModel.set(DETAIL_SCREEN_MODEL_LIST, models);

        // Check that the sheet is populated properly.
        ShadowLooper.shadowMainLooper().idle();
        assertThat(getListItems().getChildCount(), is(2));

        // Check that clicks are handled properly.
        getListItemAt(0).performClick();
        ShadowLooper.shadowMainLooper().idle();
        verify(callback1, times(1)).run();
        verify(callback2, never()).run();
    }

    @Test
    @SmallTest
    public void testRecyclerViewBindsProfileDataToItemView() {
        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(sSampleProfile1, /*isSelected=*/false,
                        /*onClickListener=*/() -> {})));
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(sSampleProfile2, /*isSelected=*/true,
                        /*onClickListener=*/() -> {})));
        mModel.set(DETAIL_SCREEN_MODEL_LIST, models);

        // Check that the sheet is populated properly.
        ShadowLooper.shadowMainLooper().idle();
        assertThat(getListItems().getChildCount(), is(2));

        assertThatProfileItemLayoutIsCorrectAt(0, sSampleProfile1, /*isSelected=*/false);
        assertThatProfileItemLayoutIsCorrectAt(1, sSampleProfile2, /*isSelected=*/true);

        // Update the selection.
        models.get(0).model.set(AutofillProfileItemProperties.IS_SELECTED, true);
        models.get(1).model.set(AutofillProfileItemProperties.IS_SELECTED, false);

        ShadowLooper.shadowMainLooper().idle();

        assertThatProfileItemLayoutIsCorrectAt(0, sSampleProfile1, /*isSelected=*/true);
        assertThatProfileItemLayoutIsCorrectAt(1, sSampleProfile2, /*isSelected=*/false);
    }

    @Test
    @SmallTest
    public void testRecyclerViewBindsCreditCardDataToItemView() {
        ModelList models = mModel.get(CREDIT_CARD_MODEL_LIST);
        FastCheckoutCreditCard sampleCardNoName =
                FastCheckoutTestUtils.createDetailedCreditCard(/*guid=*/"123",
                        /*origin=*/"https://example.at", /*name=*/"", /*number=*/"23423423432",
                        /*obfuscatedNumber=*/"34326", /*month=*/"05", /*year=*/"2035",
                        /*issuerIconString=*/"visaCC");

        models.add(new ListItem(DetailItemType.CREDIT_CARD,
                CreditCardItemProperties.create(sSampleCard1, /*isSelected=*/false,
                        /*onClickListener=*/() -> {})));
        models.add(new ListItem(DetailItemType.CREDIT_CARD,
                CreditCardItemProperties.create(sSampleCard2, /*isSelected=*/true,
                        /*onClickListener=*/() -> {})));
        models.add(new ListItem(DetailItemType.CREDIT_CARD,
                CreditCardItemProperties.create(sampleCardNoName, /*isSelected=*/false,
                        /*onClickListener=*/() -> {})));
        mModel.set(DETAIL_SCREEN_MODEL_LIST, models);

        // Check that the sheet is populated properly.
        ShadowLooper.shadowMainLooper().idle();
        assertThat(getListItems().getChildCount(), is(3));

        assertThatCreditCardItemLayoutIsCorrectAt(0, sSampleCard1, /*isSelected=*/false);
        assertThatCreditCardItemLayoutIsCorrectAt(1, sSampleCard2, /*isSelected=*/true);
        assertThatCreditCardItemLayoutIsCorrectAt(2, sampleCardNoName, /*isSelected=*/false);

        // Update the selection.
        models.get(0).model.set(CreditCardItemProperties.IS_SELECTED, true);
        models.get(1).model.set(CreditCardItemProperties.IS_SELECTED, false);

        ShadowLooper.shadowMainLooper().idle();

        assertThatCreditCardItemLayoutIsCorrectAt(0, sSampleCard1, /*isSelected=*/true);
        assertThatCreditCardItemLayoutIsCorrectAt(1, sSampleCard2, /*isSelected=*/false);
        assertThatCreditCardItemLayoutIsCorrectAt(2, sampleCardNoName, /*isSelected=*/false);
    }

    @Test
    @SmallTest
    public void testRecyclerViewHandlesFooterItemCorrectly() {
        Runnable callback = mock(Runnable.class);

        ModelList models = mModel.get(PROFILE_MODEL_LIST);
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(sSampleProfile1, /*isSelected=*/false,
                        /*onClickListener=*/() -> { Assert.fail(); })));
        models.add(new ListItem(DetailItemType.FOOTER,
                FooterItemProperties.create(
                        /*label=*/R.string.fast_checkout_detail_screen_add_autofill_profile_text,
                        /*onClickHandler=*/callback)));
        mModel.set(DETAIL_SCREEN_MODEL_LIST, models);

        // Check that the sheet is populated properly.
        ShadowLooper.shadowMainLooper().idle();
        assertThat(getListItems().getChildCount(), is(2));

        // Check that the correct text is set for the footer item.
        assertThat(getTextFromListItemWithId(1, R.id.fast_checkout_add_new_item_label),
                equalTo(mView.getContext().getResources().getString(
                        R.string.fast_checkout_detail_screen_add_autofill_profile_text)));

        // Check that clicks are handled properly.
        getListItemAt(1).performClick();
        ShadowLooper.shadowMainLooper().idle();
        verify(callback, times(1)).run();
    }

    private RecyclerView getListItems() {
        return mView.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
    }

    private View getListItemAt(int index) {
        return getListItems().getChildAt(index);
    }

    /** Returns the text contained in the item with resId inside the item at this index. */
    private String getTextFromListItemWithId(int index, int resId) {
        TextView textView = getListItemAt(index).findViewById(resId);
        return textView.getText().toString();
    }

    /** Asserts that the layout of the profile item at the given index is correct. */
    private void assertThatProfileItemLayoutIsCorrectAt(
            int index, FastCheckoutAutofillProfile profile, boolean isSelected) {
        assertThat(getTextFromListItemWithId(index, R.id.fast_checkout_autofill_profile_item_name),
                equalTo(profile.getFullName()));
        assertThat(getTextFromListItemWithId(
                           index, R.id.fast_checkout_autofill_profile_item_street_address),
                equalTo(profile.getStreetAddress()));
        assertThat(getTextFromListItemWithId(
                           index, R.id.fast_checkout_autofill_profile_item_city_and_postal_code),
                equalTo(profile.getLocality() + ", " + profile.getPostalCode()));
        assertThat(
                getTextFromListItemWithId(index, R.id.fast_checkout_autofill_profile_item_country),
                equalTo(profile.getCountryName()));
        assertThat(getTextFromListItemWithId(index, R.id.fast_checkout_autofill_profile_item_email),
                equalTo(profile.getEmailAddress()));
        assertThat(getTextFromListItemWithId(
                           index, R.id.fast_checkout_autofill_profile_item_phone_number),
                equalTo(profile.getPhoneNumber()));

        View icon = getListItemAt(index).findViewById(
                R.id.fast_checkout_autofill_profile_item_selected_icon);
        assertThat(icon.getContentDescription(),
                equalTo(mView.getContext().getResources().getString(
                        R.string.fast_checkout_detail_screen_selected_icon_description)));
        assertThat(icon.getVisibility(), is(isSelected ? View.VISIBLE : View.GONE));
    }

    /** Asserts that the layout of the credit card item at the given index is correct. */
    private void assertThatCreditCardItemLayoutIsCorrectAt(
            int index, FastCheckoutCreditCard card, boolean isSelected) {
        assertThat(getTextFromListItemWithId(index, R.id.fast_checkout_credit_card_item_number),
                equalTo(card.getObfuscatedNumber()));

        // The name row should get hidden if the name is empty.
        TextView nameView =
                getListItemAt(index).findViewById(R.id.fast_checkout_credit_card_item_name);
        if (card.getName().isEmpty()) {
            assertThat(nameView.getVisibility(), is(View.INVISIBLE));
        } else {
            assertThat(nameView.getVisibility(), is(View.VISIBLE));
            assertThat(nameView.getText().toString(), equalTo(card.getName()));
        }

        assertThat(getTextFromListItemWithId(
                           index, R.id.fast_checkout_credit_card_item_expiration_date),
                equalTo(card.getMonth() + "/" + card.getYear()));

        View icon = getListItemAt(index).findViewById(
                R.id.fast_checkout_credit_card_item_selected_icon);
        assertThat(icon.getVisibility(), is(isSelected ? View.VISIBLE : View.GONE));

        // Check that the icon is the correct one.
        ImageView paymentIcon =
                getListItemAt(index).findViewById(R.id.fast_checkout_credit_card_icon);
        assertThat(shadowOf(paymentIcon.getDrawable()).getCreatedFromResId(),
                is(card.getIssuerIconDrawableId()));
    }
}
