// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.Px;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.LoginButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
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
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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

    static class MockModalDialogManager extends ModalDialogManager {
        private PropertyModel mDialogModel;
        private @ModalDialogManager.ModalDialogType int mDialogType;

        public MockModalDialogManager() {
            super(Mockito.mock(ModalDialogManager.Presenter.class), 0);
        }

        @Override
        public void showDialog(
                PropertyModel model,
                @ModalDialogManager.ModalDialogType int dialogType,
                boolean showNext) {
            mDialogModel = model;
            mDialogType = dialogType;
        }

        public PropertyModel getDialogModel() {
            return mDialogModel;
        }

        public @ModalDialogManager.ModalDialogType int getDialogType() {
            return mDialogType;
        }

        public void simulateButtonClick(@ModalDialogProperties.ButtonType int buttonType) {
            mDialogModel.get(ModalDialogProperties.CONTROLLER).onClick(mDialogModel, buttonType);
        }

        @Override
        public void dismissDialog(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            assertEquals(model, mDialogModel);
            mDialogModel
                    .get(ModalDialogProperties.CONTROLLER)
                    .onDismiss(mDialogModel, dismissalCause);
            mDialogModel = null;
            mDialogType = -1;
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
    protected static final float ALPHA_COMPARISON_DELTA = 0.00001f;

    @Mock Callback<ButtonData> mAccountCallback;
    @Mock Callback<ButtonData> mIdpLoginCallback;
    @Mock AccountSelectionComponent.Delegate mMockDelegate;
    @Mock AccountSelectionMediator.ActiveSheetSwitcher mMockActiveSheetSwitcher;
    @Mock BottomSheetController mMockBottomSheetController;
    @Mock Tab mTab;
    Context mContext;
    MockModalDialogManager mMockModalDialogManager;

    // Constants but this test base is used by parameterized tests. These can only be initialized
    // after parameterized test runner setup.
    String mTestEtldPlusOne;
    String mTestEtldPlusOne1;
    String mTestEtldPlusOne2;
    GURL mTestUrlTermsOfService;
    GURL mTestUrlPrivacyPolicy;
    GURL mTestIdpBrandIconUrl;
    GURL mTestRpBrandIconUrl;
    GURL mTestConfigUrl;
    GURL mTestConfigUrl2;
    GURL mTestLoginUrl;
    GURL mTestLoginUrl2;
    GURL mTestIdpAccountManagementUrl;
    GURL mTestIdpPrivacyPolicyUrl;
    GURL mTestIdpTermsOfServiceUrl;
    GURL mTestErrorUrl;
    GURL mTestEmptyErrorUrl;
    Account mAnaAccount;
    Account mAnaAccountWithUseDifferentAccount;
    Account mAnaAccountWithoutBrandIcons;
    Account mBobAccount;
    Account mCarlAccount;
    Account mDanAccount;
    Account mNewUserAccount;
    Account mNewUserAccountWithoutFields;
    Account mNewUserAccountForIdp2;
    Account mNoOneAccount;
    Account mFilteredOutAccount;
    Account mFilteredOutAccountWithUseDifferentAccount;
    Account mFilteredOutAccountForIdp2;
    Account mNicolasAccount;
    Account mSingleIdentifierAccount;
    Account mSingleIdentifierAccountFilteredOut;

    IdentityCredentialTokenError mTokenError;
    IdentityCredentialTokenError mTokenErrorEmptyUrl;

    Resources mResources;
    PropertyModel mModel;
    ModelList mSheetAccountItems;
    View mContentView;
    IdentityProviderMetadata mIdpMetadata;
    IdentityProviderMetadata mIdpMetadataWithoutIcon;
    IdentityProviderData mIdpData;
    // A second IDP, for multi IDP tests. It supports use a different account.
    IdentityProviderData mIdpData2;
    IdentityProviderData mIdpDataWithoutIcons;
    IdentityProviderMetadata mIdpMetadataWithUseDifferentAccount;
    IdentityProviderData mIdpDataWithUseDifferentAccount;
    List<Account> mNewAccountsSingleReturningAccount;
    List<Account> mNewAccountsSingleNewAccount;
    List<Account> mNewAccountsMultipleAccounts;
    AccountSelectionBottomSheetContent mBottomSheetContent;
    AccountSelectionMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of
        // this test.
        mTestEtldPlusOne = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        mTestEtldPlusOne1 = JUnitTestGURLs.URL_1.getSpec();
        mTestEtldPlusOne2 = JUnitTestGURLs.URL_2.getSpec();
        mTestUrlTermsOfService = JUnitTestGURLs.RED_1;
        mTestUrlPrivacyPolicy = JUnitTestGURLs.RED_2;
        mTestIdpBrandIconUrl = JUnitTestGURLs.RED_3;
        mTestRpBrandIconUrl = JUnitTestGURLs.RED_3;
        mTestConfigUrl = new GURL("https://idp.com/fedcm.json");
        mTestConfigUrl2 = new GURL("https://idp2.com/fedcm.json");
        mTestLoginUrl = new GURL("https://idp.com/login");
        mTestLoginUrl2 = new GURL("https://idp2.com/login");
        mTestIdpAccountManagementUrl = new GURL("https://idp.com/account");
        mTestIdpPrivacyPolicyUrl = new GURL("https://idp.com/privacy");
        mTestIdpTermsOfServiceUrl = new GURL("https://idp.com/terms");
        mTestErrorUrl = new GURL("https://idp.com/error");
        mTestEmptyErrorUrl = new GURL("");

        mTokenError = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestErrorUrl);
        mTokenErrorEmptyUrl = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestEmptyErrorUrl);

        mIdpMetadata =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_4444),
                        mTestConfigUrl,
                        mTestLoginUrl,
                        mTestIdpAccountManagementUrl,
                        mTestIdpPrivacyPolicyUrl,
                        mTestIdpTermsOfServiceUrl,
                        /* showUseDifferentAccountButton= */ false);

        mIdpMetadataWithoutIcon =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        null,
                        mTestConfigUrl,
                        mTestLoginUrl,
                        /* accountManagementUrl= */ GURL.emptyGURL(),
                        /* privacyPolicyUrl= */ GURL.emptyGURL(),
                        /* termsOfServiceUrl= */ GURL.emptyGURL(),
                        /* showUseDifferentAccountButton= */ false);

        mIdpData =
                new IdentityProviderData(
                        mTestEtldPlusOne2,
                        mIdpMetadata,
                        new ClientIdMetadata(
                                mTestUrlTermsOfService,
                                mTestUrlPrivacyPolicy,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);

        mIdpData2 =
                new IdentityProviderData(
                        mTestEtldPlusOne1,
                        new IdentityProviderMetadata(
                                Color.RED,
                                Color.YELLOW,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_4444),
                                mTestConfigUrl2,
                                mTestLoginUrl2,
                                /* accountManagementUrl= */ GURL.emptyGURL(),
                                /* privacyPolicyUrl= */ GURL.emptyGURL(),
                                /* termsOfServiceUrl= */ GURL.emptyGURL(),
                                /* showUseDifferentAccountButton= */ true),
                        new ClientIdMetadata(
                                mTestUrlTermsOfService,
                                mTestUrlPrivacyPolicy,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);

        mIdpDataWithoutIcons =
                new IdentityProviderData(
                        mTestEtldPlusOne2,
                        mIdpMetadataWithoutIcon,
                        new ClientIdMetadata(mTestUrlTermsOfService, mTestUrlPrivacyPolicy, null),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);

        mIdpMetadataWithUseDifferentAccount =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        null,
                        mTestConfigUrl,
                        mTestLoginUrl,
                        /* accountManagementUrl= */ GURL.emptyGURL(),
                        /* privacyPolicyUrl= */ GURL.emptyGURL(),
                        /* termsOfServiceUrl= */ GURL.emptyGURL(),
                        /* showUseDifferentAccountButton= */ true);
        mIdpDataWithUseDifferentAccount =
                new IdentityProviderData(
                        mTestEtldPlusOne2,
                        mIdpMetadataWithUseDifferentAccount,
                        new ClientIdMetadata(mTestUrlTermsOfService, mTestUrlPrivacyPolicy, null),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);

        mAnaAccount =
                new Account(
                        "Ana",
                        "ana@email.example",
                        "Ana Doe",
                        "Ana",
                        /* secondaryDescription= */ "email.example",
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);
        mAnaAccountWithUseDifferentAccount =
                new Account(
                        "Ana",
                        "ana@email.example",
                        "Ana Doe",
                        "Ana",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpDataWithUseDifferentAccount);
        mAnaAccountWithoutBrandIcons =
                new Account(
                        "Ana",
                        "ana@email2.example",
                        "Ana Doe",
                        "Ana",
                        /* secondaryDescription= */ "email2.example",
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpDataWithoutIcons);
        mBobAccount =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);
        mCarlAccount =
                new Account(
                        "Carl",
                        "carl@three.test",
                        "Carl Test",
                        ":)",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);
        mDanAccount =
                new Account(
                        "Dan",
                        "dan@idp2.example",
                        "Dan Doe",
                        "Dan",
                        /* secondaryDescription= */ mTestEtldPlusOne1,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData2);
        mNewUserAccount =
                new Account(
                        "602214076",
                        "goto@email.example",
                        "Sam E. Goto",
                        "Sam",
                        /* secondaryDescription= */ "email.example",
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        DEFAULT_DISCLOSURE_FIELDS,
                        mIdpData);

        mNewUserAccountWithoutFields =
                new Account(
                        "602214076",
                        "goto@email.example",
                        "Sam E. Goto",
                        "Sam",
                        /* secondaryDescription= */ "email.example",
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);

        mNewUserAccountForIdp2 =
                new Account(
                        "Eve",
                        "eve@idp2.example",
                        "Eve Doe",
                        "Eve",
                        /* secondaryDescription= */ mTestEtldPlusOne1,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        DEFAULT_DISCLOSURE_FIELDS,
                        mIdpData2);

        mNoOneAccount =
                new Account(
                        "",
                        "",
                        "No Subject",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);
        mFilteredOutAccount =
                new Account(
                        "ID123",
                        "nicolas@example.com",
                        "Nicolas Pena",
                        "Nicolas",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ true,
                        /* fields= */ new int[0],
                        mIdpData);
        mFilteredOutAccountWithUseDifferentAccount =
                new Account(
                        "ID123",
                        "nicolas@example.com",
                        "Nicolas Pena",
                        "Nicolas",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ true,
                        /* fields= */ new int[0],
                        mIdpDataWithUseDifferentAccount);
        mFilteredOutAccountForIdp2 =
                new Account(
                        "Fay",
                        "fay@idp2.example",
                        "Fay Doe",
                        "Fay",
                        /* secondaryDescription= */ mTestEtldPlusOne1,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ Bitmap.createBitmap(
                                100, 100, Bitmap.Config.ARGB_4444),
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ true,
                        /* fields= */ new int[0],
                        mIdpData2);
        mNicolasAccount =
                new Account(
                        "NicoId",
                        "nicolas@email.com",
                        "Nico P",
                        "Nicolas",
                        "email.com",
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        mIdpData);

        mSingleIdentifierAccount =
                new Account(
                        "singleid1",
                        "",
                        "username",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        DEFAULT_DISCLOSURE_FIELDS,
                        mIdpData);

        mSingleIdentifierAccountFilteredOut =
                new Account(
                        "singleid2",
                        "",
                        "username2",
                        "",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ true,
                        /* fields= */ new int[0],
                        mIdpData);

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
                        mContentView, mMockBottomSheetController, SupplierUtils.of(0), mRpMode);
        mMockModalDialogManager = new MockModalDialogManager();
        resetMediator();
    }

    MVCListAdapter.ListItem buildAccountItem(Account account, boolean showIdp) {
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(AccountProperties.ON_CLICK_LISTENER, mAccountCallback)
                        .with(AccountProperties.SHOW_IDP, showIdp)
                        .build());
    }

    MVCListAdapter.ListItem buildIdpLoginItem(IdentityProviderData idpData, boolean showIdp) {
        LoginButtonProperties.Properties properties =
                new LoginButtonProperties.Properties(idpData, mIdpLoginCallback, mRpMode, showIdp);
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ITEM_TYPE_LOGIN,
                new PropertyModel.Builder(LoginButtonProperties.ALL_KEYS)
                        .with(LoginButtonProperties.PROPERTIES, properties)
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
        if (key == ItemProperties.DRAGBAR_HANDLE_VISIBLE) {
            return model.get((WritableBooleanPropertyKey) key);
        }
        // All other ItemProperties keys are WritableObjectPropertyKey<PropertyModel>.
        @SuppressWarnings("unchecked")
        WritableObjectPropertyKey<PropertyModel> objectKey =
                (WritableObjectPropertyKey<PropertyModel>) key;
        return model.get(objectKey) != null;
    }

    void resetMediator() {
        mMediator =
                new AccountSelectionMediator(
                        mTab,
                        mMockDelegate,
                        mModel,
                        mSheetAccountItems,
                        mMockBottomSheetController,
                        mBottomSheetContent,
                        DESIRED_AVATAR_SIZE,
                        mRpMode,
                        mContext,
                        mMockModalDialogManager,
                        /* canShowUi= */ true,
                        mRpMode == RpMode.PASSIVE ? mMockActiveSheetSwitcher : null);
    }

    /** Clicks the continue button the same way the view does. */
    void clickContinueButton() {
        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        ContinueButtonProperties.Properties continueButton =
                mModel.get(ItemProperties.CONTINUE_BUTTON).get(ContinueButtonProperties.PROPERTIES);
        continueButton.mOnClickListener.onResult(
                new ButtonData(continueButton.mAccount, continueButton.mIdpMetadata));
    }
}
