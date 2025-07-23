// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Common test fixtures for AccountSelectionIntegration Android Javatests. */
public class AccountSelectionIntegrationTestBase {
    protected static final String EXAMPLE_ETLD_PLUS_ONE = "example.com";
    protected static final String TEST_ETLD_PLUS_ONE_2 = "two.com";
    protected static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    protected static final IdentityProviderMetadata IDP_METADATA =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconBitmap= */ Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_4444),
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* showUseDifferentAccountButton= */ false);
    protected static final IdentityProviderMetadata IDP_METADATA_WITH_ADD_ACCOUNT =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconBitmap= */ Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_4444),
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* showUseDifferentAccountButton= */ true);

    protected static final @IdentityRequestDialogDisclosureField int[] DEFAULT_DISCLOSURE_FIELDS =
            new int[] {
                IdentityRequestDialogDisclosureField.NAME,
                IdentityRequestDialogDisclosureField.EMAIL,
                IdentityRequestDialogDisclosureField.PICTURE
            };

    protected static final String TEST_ERROR_CODE = "invalid_request";
    protected static final IdentityCredentialTokenError TOKEN_ERROR =
            new IdentityCredentialTokenError(TEST_ERROR_CODE, TEST_URL);

    AccountSelectionCoordinator mAccountSelection;

    @Mock AccountSelectionComponent.Delegate mMockBridge;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    WebPageStation mPage;
    BottomSheetController mBottomSheetController;

    String mTestUrlTermsOfService;
    String mTestUrlPrivacyPolicy;
    ClientIdMetadata mClientIdMetadata;
    List<Account> mNewAccountsReturningAna;
    List<Account> mNewAccountsNewBob;
    @RpMode.EnumType int mRpMode;
    IdentityProviderData mIdpData;
    IdentityProviderData mIdpDataWithAddAccount;
    Account mReturningAna;
    Account mNewBob;
    Account mReturningAnaWithAddAccount;
    Account mNewBobWithAddAccount;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mPage = mActivityTestRule.startOnBlankPage();

        mTestUrlTermsOfService =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title1.html");
        mTestUrlPrivacyPolicy =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title2.html");
        mClientIdMetadata =
                new ClientIdMetadata(
                        new GURL(mTestUrlTermsOfService),
                        new GURL(mTestUrlPrivacyPolicy),
                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        mIdpData =
                new IdentityProviderData(
                        TEST_ETLD_PLUS_ONE_2,
                        IDP_METADATA,
                        mClientIdMetadata,
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);
        mIdpDataWithAddAccount =
                new IdentityProviderData(
                        TEST_ETLD_PLUS_ONE_2,
                        IDP_METADATA_WITH_ADD_ACCOUNT,
                        mClientIdMetadata,
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);

        mReturningAna =
                new Account(
                        "Ana",
                        "ana@one.test",
                        "Ana Doe",
                        "Ana",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);
        mNewBob =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        DEFAULT_DISCLOSURE_FIELDS,
                        mIdpData);

        mReturningAnaWithAddAccount =
                new Account(
                        "Ana",
                        "ana@one.test",
                        "Ana Doe",
                        "Ana",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpDataWithAddAccount);
        mNewBobWithAddAccount =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        DEFAULT_DISCLOSURE_FIELDS,
                        mIdpDataWithAddAccount);

        mNewAccountsReturningAna = Arrays.asList(mReturningAna);
        mNewAccountsNewBob = Arrays.asList(mNewBob);

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mAccountSelection =
                            new AccountSelectionCoordinator(
                                    mActivityTestRule.getActivity().getActivityTab(),
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mBottomSheetController,
                                    mRpMode,
                                    mMockBridge);
                });
    }

    @SheetState
    int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    public static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }
}
