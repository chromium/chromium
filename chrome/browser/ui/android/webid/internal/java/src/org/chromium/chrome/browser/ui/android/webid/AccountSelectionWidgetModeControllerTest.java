// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;

import java.util.Arrays;

/** Controller tests verify that the Account Selection Widget Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionWidgetModeControllerTest extends AccountSelectionJUnitTestBase {
    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.WIDGET;
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

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertFalse(containsItemOfType(mModel, ItemProperties.SPINNER_ENABLED));
        }
    }
}
