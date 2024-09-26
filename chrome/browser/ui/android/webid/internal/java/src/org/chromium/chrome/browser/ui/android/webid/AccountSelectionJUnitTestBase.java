// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.res.Resources;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.Px;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Common test fixtures for AccountSelection Robolectric JUnit tests. */
public class AccountSelectionJUnitTestBase {
    @Parameter(0)
    public @RpMode.EnumType int mRpMode;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    static class RpContextEntry {
        public int mValue;
        public int mTitleId;

        RpContextEntry(@RpContext.EnumType int value, int titleId) {
            mValue = value;
            mTitleId = titleId;
        }
    }

    protected static final String TEST_ERROR_CODE = "invalid_request";
    protected static final int[] RP_CONTEXTS =
            new int[] {RpContext.SIGN_IN, RpContext.SIGN_UP, RpContext.USE, RpContext.CONTINUE};
    protected static final @Px int DESIRED_AVATAR_SIZE = 100;
    protected static final @IdentityRequestDialogDisclosureField int[] DEFAULT_DISCLOSURE_FIELDS =
            new int[] {
                IdentityRequestDialogDisclosureField.NAME,
                IdentityRequestDialogDisclosureField.EMAIL,
                IdentityRequestDialogDisclosureField.PICTURE
            };

    @Mock Callback<Account> mAccountCallback;
    @Mock AccountSelectionComponent.Delegate mMockDelegate;
    @Mock ImageFetcher mMockImageFetcher;
    @Mock BottomSheetController mMockBottomSheetController;
    @Mock Tab mTab;

    // Constants but this test base is used by parameterized tests. These can only be initialized
    // after parameterized test runner setup.
    String mTestEtldPlusOne;
    String mTestEtldPlusOne1;
    String mTestEtldPlusOne2;
    GURL mTestUrlTermsOfService;
    GURL mTestUrlPrivacyPolicy;
    GURL mTestIdpBrandIconUrl;
    GURL mTestRpBrandIconUrl;
    GURL mTestProfilePicUrl;
    GURL mTestConfigUrl;
    GURL mTestLoginUrl;
    GURL mTestErrorUrl;
    GURL mTestEmptyErrorUrl;
    Account mAnaAccount;
    Account mBobAccount;
    Account mCarlAccount;
    Account mNewUserAccount;
    Account mNoOneAccount;

    IdentityCredentialTokenError mTokenError;
    IdentityCredentialTokenError mTokenErrorEmptyUrl;

    Resources mResources;
    PropertyModel mModel;
    ModelList mSheetAccountItems;
    View mContentView;
    IdentityProviderMetadata mIdpMetadata;
    IdentityProviderData mIdpData;
    List<Account> mNewAccountsSingleReturningAccount;
    List<Account> mNewAccountsSingleNewAccount;
    List<Account> mNewAccountsMultipleAccounts;
    AccountSelectionBottomSheetContent mBottomSheetContent;
    AccountSelectionMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of
        // this test.
        mTestEtldPlusOne = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        mTestEtldPlusOne1 = JUnitTestGURLs.URL_1.getSpec();
        mTestEtldPlusOne2 = JUnitTestGURLs.URL_2.getSpec();
        mTestUrlTermsOfService = JUnitTestGURLs.RED_1;
        mTestUrlPrivacyPolicy = JUnitTestGURLs.RED_2;
        mTestIdpBrandIconUrl = JUnitTestGURLs.RED_3;
        mTestRpBrandIconUrl = JUnitTestGURLs.RED_3;
        mTestProfilePicUrl = new GURL("https://profile-picture.com");
        mTestConfigUrl = new GURL("https://idp.com/fedcm.json");
        mTestLoginUrl = new GURL("https://idp.com/login");
        mTestErrorUrl = new GURL("https://idp.com/error");
        mTestEmptyErrorUrl = new GURL("");

        mAnaAccount =
                new Account(
                        "Ana",
                        "ana@email.example",
                        "Ana Doe",
                        "Ana",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true);
        mBobAccount =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true);
        mCarlAccount =
                new Account(
                        "Carl",
                        "carl@three.test",
                        "Carl Test",
                        ":)",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true);
        mNewUserAccount =
                new Account(
                        "602214076",
                        "goto@email.example",
                        "Sam E. Goto",
                        "Sam",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false);
        mNoOneAccount =
                new Account(
                        "",
                        "",
                        "No Subject",
                        "",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true);

        mTokenError = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestErrorUrl);
        mTokenErrorEmptyUrl = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestEmptyErrorUrl);

        mIdpMetadata =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        "https://icon-url.example",
                        mTestConfigUrl,
                        mTestLoginUrl,
                        /* supportsAddAccount= */ false);

        mIdpData =
                new IdentityProviderData(
                        mTestEtldPlusOne2,
                        mIdpMetadata,
                        new ClientIdMetadata(
                                mTestUrlTermsOfService,
                                mTestUrlPrivacyPolicy,
                                mTestRpBrandIconUrl.getSpec()),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* has_login_status_mismatch= */ false);

        mNewAccountsSingleReturningAccount = Arrays.asList(mAnaAccount);
        mNewAccountsSingleNewAccount = Arrays.asList(mNewUserAccount);
        mNewAccountsMultipleAccounts = Arrays.asList(mAnaAccount, mBobAccount);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mModel =
                                    new PropertyModel.Builder(
                                                    AccountSelectionProperties.ItemProperties
                                                            .ALL_KEYS)
                                            .build();
                            mSheetAccountItems = new ModelList();
                            mContentView =
                                    AccountSelectionCoordinator.setupContentView(
                                            activity, mModel, mSheetAccountItems, mRpMode);
                            activity.setContentView(mContentView);
                            mResources = activity.getResources();
                        });

        mBottomSheetContent =
                new AccountSelectionBottomSheetContent(
                        /* contentView= */ null,
                        /* bottomSheetController= */ null,
                        /* scrollOffsetSupplier= */ null,
                        mRpMode);
        mMediator =
                new AccountSelectionMediator(
                        mTab,
                        mMockDelegate,
                        mModel,
                        mSheetAccountItems,
                        mMockBottomSheetController,
                        mBottomSheetContent,
                        mMockImageFetcher,
                        DESIRED_AVATAR_SIZE,
                        mRpMode);
    }

    MVCListAdapter.ListItem buildAccountItem(Account account) {
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(AccountProperties.ON_CLICK_LISTENER, mAccountCallback)
                        .build());
    }

    int countAllItems() {
        int count = 0;
        for (PropertyKey key : mModel.getAllProperties()) {
            if (containsItemOfType(mModel, key)) {
                count += 1;
            }
        }
        return count + mSheetAccountItems.size();
    }

    static boolean containsItemOfType(PropertyModel model, PropertyKey key) {
        if (key == ItemProperties.SPINNER_ENABLED) {
            return model.get((WritableBooleanPropertyKey) key);
        }
        return model.get((WritableObjectPropertyKey<PropertyModel>) key) != null;
    }
}
