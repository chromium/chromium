// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collections;

/** Controller tests verify that the Account Selection Passive Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionWidgetModeControllerTest extends AccountSelectionJUnitTestBase {
    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.PASSIVE;
        super.setUp();
    }

    @Test
    public void testShowVerifySheetExplicitSignin() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    Arrays.asList(mNewUserAccount),
                    Arrays.asList(mIdpData),
                    /* isAutoReauthn= */ false,
                    /* newAccounts= */ Collections.EMPTY_LIST);
            mMediator.showVerifySheet(mAnaAccount);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertFalse(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
        }
    }

    @Test
    public void testShowVerifySheetAutoReauthn() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            // showVerifySheet is called in showAccounts when isAutoReauthn is true
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    Arrays.asList(mAnaAccount),
                    Arrays.asList(mIdpData),
                    /* isAutoReauthn= */ true,
                    /* newAccounts= */ Collections.EMPTY_LIST);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertFalse(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
        }
    }

    @Test
    public void testShowAccountsWithoutBrandIcons() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpDataWithoutIcons),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(IDP_BRAND_ICON));
    }

    @Test
    public void testShowErrorDialogClickGotIt() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne2,
                    mIdpMetadata,
                    rpContext,
                    mTokenErrorEmptyUrl);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.SIGN_IN_ERROR, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate, never()).onAccountsDisplayed();

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1", mTestEtldPlusOne2, errorProperties.mIdpForDisplay);
            assertEquals("Incorrect RP ETLD+1", mTestEtldPlusOne, errorProperties.mRpForDisplay);
            assertEquals("Incorrect token error", mTokenErrorEmptyUrl, errorProperties.mError);

            assertNotNull(
                    mModel.get(ItemProperties.CONTINUE_BUTTON)
                            .get(ContinueButtonProperties.PROPERTIES)
                            .mOnClickListener);
            assertNull(
                    mModel.get(ItemProperties.ERROR_TEXT)
                            .get(ErrorProperties.PROPERTIES)
                            .mMoreDetailsClickRunnable);

            // Do not let test inputs be ignored.
            mMediator.setComponentShowTime(-1000);
            mModel.get(ItemProperties.CONTINUE_BUTTON)
                    .get(ContinueButtonProperties.PROPERTIES)
                    .mOnClickListener
                    .onResult(new ButtonData(mAnaAccount, /* idpMetadata= */ null));
            verify(mMockDelegate, times(++count))
                    .onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
            assertTrue(mMediator.wasDismissed());
        }
    }

    @Test
    public void testShowErrorDialogClickMoreDetails() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne, mTestEtldPlusOne2, mIdpMetadata, rpContext, mTokenError);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.SIGN_IN_ERROR, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate, never()).onAccountsDisplayed();

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1", mTestEtldPlusOne2, errorProperties.mIdpForDisplay);
            assertEquals("Incorrect RP ETLD+1", mTestEtldPlusOne, errorProperties.mRpForDisplay);
            assertEquals("Incorrect token error", mTokenError, errorProperties.mError);

            assertNotNull(
                    mModel.get(ItemProperties.CONTINUE_BUTTON)
                            .get(ContinueButtonProperties.PROPERTIES)
                            .mOnClickListener);
            assertNotNull(
                    mModel.get(ItemProperties.ERROR_TEXT)
                            .get(ErrorProperties.PROPERTIES)
                            .mMoreDetailsClickRunnable);

            // Do not let test inputs be ignored.
            mMediator.setComponentShowTime(-1000);
            mModel.get(ItemProperties.ERROR_TEXT)
                    .get(ErrorProperties.PROPERTIES)
                    .mMoreDetailsClickRunnable
                    .run();
            verify(mMockDelegate, times(++count)).onMoreDetails();
            verify(mMockDelegate, times(count))
                    .onDismissed(IdentityRequestDialogDismissReason.MORE_DETAILS_BUTTON);
            assertTrue(mMediator.wasDismissed());
        }
    }
}
