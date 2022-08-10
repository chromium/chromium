// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutModel.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutModel.VISIBLE;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutModel.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill_assistant.AutofillAssistantPublicTags;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller tests verify that the Fast Checkout controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FastCheckoutControllerTest {
    private static final FastCheckoutAutofillProfile[] DUMMY_PROFILES = {
            createDummyProfile("John Doe", "john@gmail.com")};
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

    private final PropertyModel mModel = FastCheckoutModel.createDefaultModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator.initialize(mMockDelegate, mModel, mMockBottomSheetController);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(CURRENT_SCREEN), is(ScreenType.HOME_SCREEN));
    }

    @Test
    public void testShowOptionsSetsVisibile() {
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
