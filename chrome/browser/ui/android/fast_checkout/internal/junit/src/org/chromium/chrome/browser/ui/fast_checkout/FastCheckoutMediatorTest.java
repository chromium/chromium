// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.PROFILE_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.SELECTED_PROFILE;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.VISIBLE;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.autofill_profile_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
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
            createDummyProfile("John Doe", "john@gmail.com"),
            createDummyProfile("Jane Doe", "jane@gmail.com"),
            createDummyProfile("Foo Boo", "foo@gmail.com")};
    private static final FastCheckoutCreditCard[] DUMMY_CARDS = {
            createDummyCreditCard("https://example.com", "4111111111111111")};

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
    public void testSetCurrentScreenUpdatesModel() {
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILES_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.AUTOFILL_PROFILES_SCREEN));

        mMediator.setCurrentScreen(ScreenType.HOME_SCREEN);
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
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
    public void testSetSelectedAutofillProfileUpdatesModels() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.length));

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[0]);
        checkThatAutofillProfileIsSelected(0);

        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[2]);
        checkThatAutofillProfileIsSelected(2);
    }

    private void checkThatAutofillProfileIsSelected(int selectedIndex) {
        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        for (int index = 0; index < DUMMY_PROFILES.length; ++index) {
            PropertyModel model = models.get(index).model;
            assertThat(model.get(AutofillProfileItemProperties.IS_SELECTED),
                    is(index == selectedIndex));
        }
    }

    @Test
    public void testAutofillProfileItemOnClickListenerUpdatesSelectedProfile() {
        mMediator.setAutofillProfileItems(DUMMY_PROFILES);
        mMediator.setSelectedAutofillProfile(DUMMY_PROFILES[0]);
        mMediator.setCurrentScreen(ScreenType.AUTOFILL_PROFILES_SCREEN);

        ListModel<MVCListAdapter.ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        assertThat(models.size(), is(DUMMY_PROFILES.length));

        PropertyModel model = models.get(2).model;
        model.get(AutofillProfileItemProperties.ON_CLICK_LISTENER).run();

        assertThat(mModel.get(SELECTED_PROFILE),
                is(model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE)));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
    }

    private static FastCheckoutAutofillProfile createDummyProfile(String name, String email) {
        return new FastCheckoutAutofillProfile(/* guid= */ "", /* origin= */ "",
                /* isLocal= */ true,
                /* honorificPrefix= */ "", name,
                /* companyName= */ "", /* streetAddress= */ "", /* region= */ "",
                /* locality= */ "",
                /* dependentLocality= */ "", /* postalCode= */ "", /* sortingCode= */ "",
                /* countryCode= */ "", /* countryName= */ "", /* phoneNumber= */ "", email,
                /* languageCode= */ "en-US");
    }

    private static FastCheckoutCreditCard createDummyCreditCard(String origin, String number) {
        return new FastCheckoutCreditCard(/* guid= */ "john", origin, /* isLocal= */ true,
                /* isCached= */ true, "John Doe", number, "1111", "12", "2050", "visa",
                /* billingAddressId= */
                "",
                /* billingAddressId= */ "john",
                /* serverId= */ "",
                /* instrumentId= */ 0, /* nickname= */ "", /* cardArtUrl= */ null,
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                /* productDescription= */ "");
    }
}
