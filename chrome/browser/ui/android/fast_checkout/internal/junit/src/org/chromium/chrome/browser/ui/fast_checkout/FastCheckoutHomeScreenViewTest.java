// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.chrome.browser.ui.suggestion.Icon;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Simple unit tests for the home screen view. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class FastCheckoutHomeScreenViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private HomeScreenCoordinator.Delegate mMockDelegate;

    private View mHomeScreenView;
    private PropertyModel mModel;
    private FastCheckoutMediator mMediator;
    private static final FastCheckoutAutofillProfile sSelectedProfile =
            FastCheckoutTestUtils.createDetailedProfile(
                    /* guid= */ "111",
                    /* name= */ "John Moe",
                    /* streetAddress= */ "Park Avenue 234",
                    /* city= */ "New York",
                    /* postalCode= */ "12345",
                    /* email= */ "john.moe@gmail.com",
                    /* phoneNumber= */ "+1-345-543-645");
    private static final FastCheckoutCreditCard sSelectedCreditCard =
            FastCheckoutTestUtils.createDetailedLocalCreditCard(
                    /* guid= */ "123",
                    /* origin= */ "https://example.com",
                    /* name= */ "John Moe",
                    /* number= */ "75675675656",
                    /* obfuscatedNumber= */ "5656",
                    /* month= */ "05",
                    /* year= */ "2031",
                    /* issuerIcon= */ Icon.CARD_VISA);

    @Before
    public void setUp() {
        FeatureList.TestValues featureTestValues = new FeatureList.TestValues();
        featureTestValues.addFeatureFlagOverride(
                ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES, false);
        FeatureList.setTestValues(featureTestValues);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mModel = FastCheckoutProperties.createDefaultModel();
                            mModel.set(FastCheckoutProperties.VISIBLE, true);
                            mModel.set(FastCheckoutProperties.SELECTED_PROFILE, sSelectedProfile);
                            mModel.set(
                                    FastCheckoutProperties.SELECTED_CREDIT_CARD,
                                    sSelectedCreditCard);
                            mModel.set(FastCheckoutProperties.HOME_SCREEN_DELEGATE, mMockDelegate);

                            // Create the view.
                            mHomeScreenView =
                                    LayoutInflater.from(activity)
                                            .inflate(
                                                    R.layout.fast_checkout_home_screen_sheet, null);

                            // Let the coordinator connect model and view.
                            new HomeScreenCoordinator(activity, mHomeScreenView, mModel);
                            activity.setContentView(mHomeScreenView);
                            assertNotNull(mHomeScreenView);
                        });
    }

    @Test
    @SmallTest
    public void testViewDisplaysCorrectData() {
        assertThat(
                getTextFromView(R.id.fast_checkout_home_sheet_profile_name),
                equalTo(sSelectedProfile.getFullName()));
        assertThat(
                getTextFromView(R.id.fast_checkout_home_sheet_profile_street),
                equalTo(
                        sSelectedProfile.getStreetAddress()
                                + ", "
                                + sSelectedProfile.getPostalCode()));
        assertThat(
                getTextFromView(R.id.fast_checkout_home_sheet_profile_email),
                equalTo(sSelectedProfile.getEmailAddress()));
        assertThat(
                getTextFromView(R.id.fast_checkout_home_sheet_profile_phone_number),
                equalTo(sSelectedProfile.getPhoneNumber()));
        assertThat(
                getTextFromView(R.id.fast_checkout_sheet_selected_credit_card_header),
                equalTo(sSelectedCreditCard.getObfuscatedNumber()));

        // Test the accessibility strings for the "expand" icons.
        assertThat(
                getContentDescriptionFromView(R.id.fast_checkout_expand_icon_autofill_profile),
                equalTo(
                        mHomeScreenView
                                .getContext()
                                .getResources()
                                .getString(
                                        R.string
                                                .fast_checkout_home_sheet_expand_icon_autofill_profile_description)));
        assertThat(
                getContentDescriptionFromView(R.id.fast_checkout_expand_icon_credit_card),
                equalTo(
                        mHomeScreenView
                                .getContext()
                                .getResources()
                                .getString(
                                        R.string
                                                .fast_checkout_home_sheet_expand_icon_credit_card_description)));

        ImageView mCreditCardImageView =
                (ImageView) mHomeScreenView.findViewById(R.id.fast_checkout_credit_card_icon);
        assertThat(
                shadowOf(mCreditCardImageView.getDrawable()).getCreatedFromResId(),
                equalTo(sSelectedCreditCard.getIssuerIconDrawableId()));
    }

    @Test
    @SmallTest
    public void testSelectedProfileClickOpensProfileList() {
        // Click on the profile item.
        View profileItemLayout = mHomeScreenView.findViewById(R.id.selected_address_profile_view);
        assertNotNull(profileItemLayout);
        profileItemLayout.performClick();

        ShadowLooper.shadowMainLooper().idle();
        verify(mMockDelegate, times(1)).onShowAddressesList();
    }

    @Test
    @SmallTest
    public void testSelectedCardClickOpensCreditCardsList() {
        // Click on the credit card item.
        View cardItemLayout = mHomeScreenView.findViewById(R.id.selected_credit_card_view);
        assertNotNull(cardItemLayout);
        cardItemLayout.performClick();

        ShadowLooper.shadowMainLooper().idle();
        verify(mMockDelegate, times(1)).onShowCreditCardList();
    }

    @Test
    @SmallTest
    public void testAcceptingHomeScreenOptions() {
        // Click on accept button.
        View acceptButton = mHomeScreenView.findViewById(R.id.fast_checkout_button_accept);
        assertNotNull(acceptButton);
        acceptButton.performClick();

        ShadowLooper.shadowMainLooper().idle();
        verify(mMockDelegate, times(1)).onOptionsAccepted();
    }

    /** Returns the text contained in TextView with resId inside the home screen view. */
    private String getTextFromView(int resId) {
        TextView textView = mHomeScreenView.findViewById(resId);
        return textView.getText().toString();
    }

    /** Returns the content description for the view with resId inside the home screen view. */
    private String getContentDescriptionFromView(int resId) {
        View view = mHomeScreenView.findViewById(resId);
        return view.getContentDescription().toString();
    }
}
