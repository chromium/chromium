// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
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
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
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
                    mTestEtldPlusOne2,
                    Arrays.asList(mNewUserAccount),
                    mIdpData,
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
                    mTestEtldPlusOne2,
                    Arrays.asList(mAnaAccount),
                    mIdpData,
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
    public void testShowAccountsDoesNotFetchRpIcon() {
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
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
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
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        // Brand icon should be transparent placeholder icon. This is useful so that the header text
        // wrapping does not change in the case that the brand icon download succeeds.
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
    }
}
