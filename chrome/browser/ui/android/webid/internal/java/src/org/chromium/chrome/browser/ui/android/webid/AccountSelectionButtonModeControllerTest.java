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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/** Controller tests verify that the Account Selection Button Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeControllerTest extends AccountSelectionJUnitTestBase {
    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.BUTTON;
        super.setUp();
    }

    @Test
    public void testShowVerifySheetExplicitSignin() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne2,
                    Arrays.asList(mNewUserAccount),
                    mIdpMetadata,
                    mClientIdMetadata,
                    /* isAutoReauthn= */ false,
                    rpContext,
                    /* requestPermission= */ true);
            mMediator.showVerifySheet(mAnaAccount);

            // There is no account shown in the verify sheet on button mode.
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertTrue(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
        }
    }

    @Test
    public void testShowVerifySheetAutoReauthn() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            // showVerifySheet is called in showAccounts when isAutoReauthn is true
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne2,
                    Arrays.asList(mAnaAccount),
                    mIdpMetadata,
                    mClientIdMetadata,
                    /* isAutoReauthn= */ true,
                    rpContext,
                    /* requestPermission= */ true);

            // There is no account shown in the verify sheet on button mode.
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertTrue(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
        }
    }

    @Test
    public void testShowLoadingDialog() {
        // Button flow can be triggered regardless of the requestShowContent result.
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                .thenReturn(anyBoolean());
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne1, RpContext.SIGN_IN);
        assertEquals(0, mSheetAccountItems.size());
        assertEquals(HeaderType.LOADING, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockDelegate, never()).onAccountsDisplayed();

        // For loading dialog, we expect header + spinner.
        assertEquals(2, countAllItems());
        assertTrue(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));

        // Switching to accounts dialog should disable the spinner.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                RpContext.SIGN_IN,
                /* requestPermission= */ true);
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));

        // For accounts dialog, we expect header + two accounts.
        assertEquals(3, countAllItems());
        assertFalse(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
    }

    @Test
    public void testShowRequestPermissionDialog() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                RpContext.SIGN_IN,
                /* requestPermission= */ true);
        mMediator.showRequestPermissionSheet(mNewUserAccount);

        // For request permission dialog, we expect header + account chip + disclosure text +
        // continue button.
        assertEquals(4, countAllItems());

        // There is no sheet account items because the account is shown in an account chip instead.
        assertEquals(0, mSheetAccountItems.size());
        assertEquals(HeaderType.REQUEST_PERMISSION, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertTrue(containsItemOfType(mModel, ItemProperties.ACCOUNT_CHIP));
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    @Test
    public void testShowAccountsFetchesRpIcon() {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback<Bitmap> callback =
                                        (Callback<Bitmap>) invocation.getArguments()[1];

                                Bitmap brandIcon =
                                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                                brandIcon.eraseColor(Color.RED);
                                callback.onResult(brandIcon);
                                return null;
                            }
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any(Callback.class));

        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                RpContext.SIGN_IN,
                /* requestPermission= */ true);

        assertNotNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }

    @Test
    public void testBrandIconDownloadFails() {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback<Bitmap> callback =
                                        (Callback<Bitmap>) invocation.getArguments()[1];
                                callback.onResult(null);
                                return null;
                            }
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any(Callback.class));

        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                RpContext.SIGN_IN,
                /* requestPermission= */ true);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        // Unlike widget mode, brand icons should not be available because we do not show any
        // placeholder icon.
        assertNull(headerModel.get(IDP_BRAND_ICON));
        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }
}
