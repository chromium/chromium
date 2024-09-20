// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

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
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Common test fixtures for AccountSelectionIntegration Android Javatests. */
public class AccountSelectionIntegrationTestBase {
    protected static final String EXAMPLE_ETLD_PLUS_ONE = "example.com";
    protected static final String TEST_ETLD_PLUS_ONE_2 = "two.com";
    protected static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.URL_1_WITH_PATH;
    protected static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    protected static final Account RETURNING_ANA =
            new Account(
                    "Ana",
                    "ana@one.test",
                    "Ana Doe",
                    "Ana",
                    TEST_PROFILE_PIC,
                    null,
                    /* isSignIn= */ true,
                    /* isBrowserTrustedSignIn= */ true);
    protected static final Account NEW_BOB =
            new Account(
                    "Bob",
                    "",
                    "Bob",
                    "",
                    TEST_PROFILE_PIC,
                    null,
                    /* isSignIn= */ false,
                    /* isBrowserTrustedSignIn= */ false);

    protected static final IdentityProviderMetadata IDP_METADATA =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconUrl= */ EXAMPLE_ETLD_PLUS_ONE,
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* supportsAddAccount= */ false);
    protected static final IdentityProviderMetadata IDP_METADATA_WITH_ADD_ACCOUNT =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconUrl= */ EXAMPLE_ETLD_PLUS_ONE,
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* supportsAddAccount= */ true);

    protected static final @IdentityRequestDialogDisclosureField int[] DEFAULT_DISCLOSURE_FIELDS =
            new int[] {
                IdentityRequestDialogDisclosureField.NAME,
                IdentityRequestDialogDisclosureField.EMAIL,
                IdentityRequestDialogDisclosureField.PICTURE
            };

    AccountSelectionCoordinator mAccountSelection;

    @Mock AccountSelectionComponent.Delegate mMockBridge;
    @Mock ImageFetcher mMockImageFetcher;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    BottomSheetController mBottomSheetController;

    String mTestUrlTermsOfService;
    String mTestUrlPrivacyPolicy;
    ClientIdMetadata mClientIdMetadata;
    List<Account> mNewAccountsReturningAna;
    List<Account> mNewAccountsNewBob;
    @RpMode.EnumType int mRpMode;
    IdentityProviderData mIdpData;
    IdentityProviderData mIdpDataWithAddAccount;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();

        mTestUrlTermsOfService =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title1.html");
        mTestUrlPrivacyPolicy =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title2.html");
        mClientIdMetadata =
                new ClientIdMetadata(
                        new GURL(mTestUrlTermsOfService),
                        new GURL(mTestUrlPrivacyPolicy),
                        EXAMPLE_ETLD_PLUS_ONE);
        mNewAccountsReturningAna = Arrays.asList(RETURNING_ANA);
        mNewAccountsNewBob = Arrays.asList(NEW_BOB);

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
                    mAccountSelection.getMediator().setImageFetcher(mMockImageFetcher);
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
