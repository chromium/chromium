// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static java.util.Arrays.asList;

import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet. This class is parameterized to run all tests for each RP mode.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class AccountSelectionViewTest extends AccountSelectionJUnitTestBase {
    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {RpMode.PASSIVE, RpMode.ACTIVE});
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of this
    // test.
    private static final String TEST_IDP_ETLD_PLUS_ONE = "https://idp.com";
    private static final String TEST_RP_ETLD_PLUS_ONE = "https://rp.com";

    // Android chrome strings may have link tags which needs to be removed before comparing with the
    // actual text on the dialog.
    private static final String LINK_TAG_REGEX = "<[^>]*>";

    private class TokenError {
        public final String mCode;
        public final GURL mUrl;
        public final String mExpectedSummary;
        public final String mExpectedDescription;
        public final boolean mClickableText;

        TokenError(String code, GURL url, boolean clickableText) {
            mCode = code;
            mUrl = url;
            mExpectedSummary = mCodeToSummary.get(code);
            mExpectedDescription =
                    AccountSelectionViewBinder.SERVER_ERROR.equals(code)
                            ? mCodeToDescription.get(code)
                            : appendExtraDescription(code, url, clickableText);
            mClickableText = clickableText;
        }

        private final Map<String, String> mCodeToSummary =
                Map.of(
                        AccountSelectionViewBinder.GENERIC,
                        mResources.getString(
                                R.string.signin_generic_error_dialog_summary,
                                TEST_IDP_ETLD_PLUS_ONE),
                        AccountSelectionViewBinder.INVALID_REQUEST,
                        mResources.getString(
                                R.string.signin_invalid_request_error_dialog_summary,
                                TEST_RP_ETLD_PLUS_ONE,
                                TEST_IDP_ETLD_PLUS_ONE),
                        AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                        mResources.getString(
                                R.string.signin_unauthorized_client_error_dialog_summary,
                                TEST_RP_ETLD_PLUS_ONE,
                                TEST_IDP_ETLD_PLUS_ONE),
                        AccountSelectionViewBinder.ACCESS_DENIED,
                        mResources.getString(R.string.signin_access_denied_error_dialog_summary),
                        AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                        mResources.getString(
                                R.string.signin_temporarily_unavailable_error_dialog_summary),
                        AccountSelectionViewBinder.SERVER_ERROR,
                        mResources.getString(R.string.signin_server_error_dialog_summary));

        private final Map<String, String> mCodeToDescription =
                Map.of(
                        AccountSelectionViewBinder.GENERIC,
                        mResources.getString(R.string.signin_generic_error_dialog_description),
                        AccountSelectionViewBinder.INVALID_REQUEST,
                        mResources.getString(
                                R.string.signin_invalid_request_error_dialog_description),
                        AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                        mResources.getString(
                                R.string.signin_unauthorized_client_error_dialog_description),
                        AccountSelectionViewBinder.ACCESS_DENIED,
                        mResources.getString(
                                R.string.signin_access_denied_error_dialog_description),
                        AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                        mResources.getString(
                                R.string.signin_temporarily_unavailable_error_dialog_description,
                                TEST_IDP_ETLD_PLUS_ONE),
                        AccountSelectionViewBinder.SERVER_ERROR,
                        mResources.getString(
                                R.string.signin_server_error_dialog_description,
                                TEST_RP_ETLD_PLUS_ONE));

        private final String appendExtraDescription(String code, GURL url, boolean clickableText) {
            String initialDescription = mCodeToDescription.get(code);
            if (AccountSelectionViewBinder.GENERIC.equals(code)) {
                if (mTestEmptyErrorUrl.equals(url) || !clickableText) {
                    return initialDescription;
                }
                return initialDescription
                        + ". "
                        + mResources
                                .getString(R.string.signin_generic_error_dialog_more_details_prompt)
                                .replaceAll(LINK_TAG_REGEX, "");
            }

            if (mTestEmptyErrorUrl.equals(url)) {
                return initialDescription
                        + " "
                        + mResources.getString(
                                AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE.equals(code)
                                        ? R.string.signin_error_dialog_try_other_ways_retry_prompt
                                        : R.string.signin_error_dialog_try_other_ways_prompt,
                                TEST_RP_ETLD_PLUS_ONE);
            }
            return initialDescription
                    + " "
                    + mResources
                            .getString(
                                    AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE.equals(code)
                                            ? (clickableText
                                                    ? R.string
                                                            .signin_error_dialog_more_details_retry_prompt
                                                    : R.string
                                                            .signin_error_dialog_more_details_button_retry_prompt)
                                            : (clickableText
                                                    ? R.string
                                                            .signin_error_dialog_more_details_prompt
                                                    : R.string
                                                            .signin_error_dialog_more_details_button_prompt),
                                    TEST_IDP_ETLD_PLUS_ONE)
                            .replaceAll(LINK_TAG_REGEX, "");
        }
    }

    @Test
    public void testAccountsChangedByModel() {
        mSheetAccountItems.addAll(
                asList(
                        buildAccountItem(mAnaAccount, /* showIdp= */ false),
                        buildAccountItem(mNoOneAccount, /* showIdp= */ false),
                        buildAccountItem(mBobAccount, /* showIdp= */ false),
                        buildAccountItem(mNicolasAccount, /* showIdp= */ false)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        assertEquals("Incorrect account count", 4, getAccounts().getChildCount());
        assertEquals(
                "Incorrect display name",
                mAnaAccount.getDisplayName(),
                getAccountNameAt(0).getText());
        assertEquals(
                "Incorrect display identifier",
                mAnaAccount.getDisplayIdentifier(),
                getAccountDescriptionAt(0).getText());
        assertEquals(
                "Should not have secondary description",
                "",
                getAccountSecondaryDescriptionAt(0).getText());
        assertEquals(View.GONE, getAccountSecondaryDescriptionAt(0).getVisibility());
        assertEquals(
                "Incorrect display name",
                mNoOneAccount.getDisplayName(),
                getAccountNameAt(1).getText());
        assertEquals(
                "Incorrect display identifier",
                mNoOneAccount.getDisplayIdentifier(),
                getAccountDescriptionAt(1).getText());
        assertEquals(
                "Should not have secondary description",
                "",
                getAccountSecondaryDescriptionAt(1).getText());
        assertEquals(View.GONE, getAccountSecondaryDescriptionAt(1).getVisibility());
        assertEquals(
                "Incorrect display name",
                mBobAccount.getDisplayName(),
                getAccountNameAt(2).getText());
        assertEquals(
                "Incorrect display identifier",
                mBobAccount.getDisplayIdentifier(),
                getAccountDescriptionAt(2).getText());
        assertEquals(
                "Should not have secondary description",
                "",
                getAccountSecondaryDescriptionAt(2).getText());
        assertEquals(View.GONE, getAccountSecondaryDescriptionAt(2).getVisibility());
        assertEquals(
                "Incorrect display name",
                mNicolasAccount.getDisplayName(),
                getAccountNameAt(3).getText());
        assertEquals(
                "Incorrect display identifier",
                mNicolasAccount.getDisplayIdentifier(),
                getAccountDescriptionAt(3).getText());

        // Even though mNicolasAccount has secondary description, it should not be shown when UI is
        // not in multi IDP mode.
        assertEquals(
                "Should not have secondary description",
                "",
                getAccountSecondaryDescriptionAt(3).getText());
        assertEquals(View.GONE, getAccountSecondaryDescriptionAt(3).getVisibility());
    }

    @Test
    public void testSecondaryDescriptionsWithMultipleIdpAccounts() {
        mSheetAccountItems.addAll(
                asList(
                        buildAccountItem(mAnaAccount, /* showIdp= */ true),
                        buildAccountItem(mAnaAccountWithoutBrandIcons, /* showIdp= */ true),
                        buildAccountItem(mNewUserAccount, /* showIdp= */ true)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        assertEquals("Incorrect account count", 3, getAccounts().getChildCount());

        TextView secondaryDescription = getAccountSecondaryDescriptionAt(0);
        assertEquals(
                "Secondary description not visible",
                View.VISIBLE,
                secondaryDescription.getVisibility());
        assertEquals(
                mAnaAccount.getSecondaryDescription() + " • You used on this site",
                secondaryDescription.getText().toString());

        secondaryDescription = getAccountSecondaryDescriptionAt(1);
        assertEquals(
                "Secondary description not visible",
                View.VISIBLE,
                secondaryDescription.getVisibility());
        assertEquals(
                mAnaAccountWithoutBrandIcons.getSecondaryDescription() + " • You used on this site",
                secondaryDescription.getText().toString());

        secondaryDescription = getAccountSecondaryDescriptionAt(2);
        assertEquals(
                "Secondary description not visible",
                View.VISIBLE,
                secondaryDescription.getVisibility());
        // This is a new account, so should not include "You used on this site".
        assertEquals(
                mNewUserAccount.getSecondaryDescription(),
                secondaryDescription.getText().toString());
    }

    @Test
    public void testAccountsAreClickable() {
        mSheetAccountItems.addAll(
                Collections.singletonList(buildAccountItem(mAnaAccount, /* showIdp= */ false)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());

        assertNotNull(getAccounts().getChildAt(0));

        getAccounts().getChildAt(0).performClick();

        ArgumentCaptor<ButtonData> captor = ArgumentCaptor.forClass(ButtonData.class);
        waitForEvent(mAccountCallback).onResult(captor.capture());
        ButtonData capturedButtonData = captor.getValue();
        assertEquals(mAnaAccount, capturedButtonData.mAccount);
        assertNull(capturedButtonData.mIdpMetadata);
    }

    @Test
    public void testSingleAccountHasClickableButton() {
        // Create an account with no callback to ensure the button callback
        // is the one that gets invoked.
        mSheetAccountItems.add(
                new MVCListAdapter.ListItem(
                        AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                        new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                .with(AccountProperties.ACCOUNT, mAnaAccount)
                                .with(AccountProperties.ON_CLICK_LISTENER, null)
                                .build()));
        ShadowLooper.shadowMainLooper().idle();

        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                buildContinueButton(mAnaAccount, mIdpMetadata, HeaderType.SIGN_IN));
        assertEquals(View.VISIBLE, mContentView.getVisibility());

        assertNotNull(getAccounts().getChildAt(0));

        View continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);
        assertTrue(continueButton.isShown());
        continueButton.performClick();

        ArgumentCaptor<ButtonData> captor = ArgumentCaptor.forClass(ButtonData.class);
        waitForEvent(mAccountCallback).onResult(captor.capture());
        ButtonData capturedButtonData = captor.getValue();
        assertEquals(mAnaAccount, capturedButtonData.mAccount);
        assertEquals(mIdpMetadata, capturedButtonData.mIdpMetadata);
    }

    @Test
    public void testDataSharingConsentDisplayed() {
        final String idpEtldPlusOne = "idp.org";
        mModel.set(
                ItemProperties.DATA_SHARING_CONSENT, buildDataSharingConsentItem(idpEtldPlusOne));
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView consent = mContentView.findViewById(R.id.user_data_sharing_consent);
        assertTrue(consent.isShown());
        String expectedSharingConsentText =
                mResources.getString(
                        R.string.account_selection_data_sharing_consent,
                        "idp.org",
                        "name, email address, and profile picture");
        expectedSharingConsentText = expectedSharingConsentText.replaceAll(LINK_TAG_REGEX, "");
        // We use toString() here because otherwise getText() returns a
        // Spanned, which is not equal to the string we get from the resources.
        assertEquals(
                "Incorrect data sharing consent text",
                expectedSharingConsentText,
                consent.getText().toString());
        Spanned spannedString = (Spanned) consent.getText();
        ClickableSpan[] spans =
                spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
        assertEquals("Expected two clickable links", 2, spans.length);
    }

    /** Tests that the brand foreground and the brand icon are used in the "Continue" button. */
    @Test
    public void testContinueButtonBranding() {
        final int expectedTextColor = Color.BLUE;
        IdentityProviderMetadata idpMetadata =
                new IdentityProviderMetadata(
                        expectedTextColor,
                        /* brandBackgroundColor= */ Color.GREEN,
                        null,
                        mTestConfigUrl,
                        mTestLoginUrl,
                        false);

        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                buildContinueButton(mAnaAccount, idpMetadata, HeaderType.SIGN_IN));

        assertEquals(View.VISIBLE, mContentView.getVisibility());

        TextView continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);

        assertEquals(expectedTextColor, continueButton.getTextColors().getDefaultColor());
    }

    @Test
    public void testIdpSignInDisplayed() {
        final String idpEtldPlusOne = "idp.org";
        mModel.set(ItemProperties.IDP_SIGNIN, buildIdpSignInItem(idpEtldPlusOne));
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView idpSignin = mContentView.findViewById(R.id.idp_signin);
        assertTrue(idpSignin.isShown());
        String expectedText =
                mResources.getString(
                        R.string.idp_signin_status_mismatch_dialog_body, idpEtldPlusOne);
        // We use toString() here because otherwise getText() returns a
        // Spanned, which is not equal to the string we get from the resources.
        assertEquals(
                "Incorrect IDP sign in mismatch body dialog text",
                expectedText,
                idpSignin.getText().toString());

        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                buildContinueButton(null, mIdpMetadata, HeaderType.SIGN_IN_TO_IDP_STATIC));
        ButtonCompat continueButton =
                mContentView.findViewById(R.id.account_selection_continue_btn);
        assertTrue(continueButton.isShown());
        assertEquals("Continue", continueButton.getText());
        assertEquals("Continue, opens in a new tab", continueButton.getContentDescription());
        continueButton.performClick();

        ArgumentCaptor<ButtonData> captor = ArgumentCaptor.forClass(ButtonData.class);
        waitForEvent(mAccountCallback).onResult(captor.capture());
        ButtonData capturedButtonData = captor.getValue();
        assertNull(capturedButtonData.mAccount);
        assertEquals(mIdpMetadata, capturedButtonData.mIdpMetadata);
    }

    @Test
    public void testErrorText() {
        final TokenError[] mErrors =
                new TokenError[] {
                    new TokenError(
                            AccountSelectionViewBinder.GENERIC,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.GENERIC,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.INVALID_REQUEST,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.INVALID_REQUEST,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.ACCESS_DENIED,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.ACCESS_DENIED,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.SERVER_ERROR,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.SERVER_ERROR,
                            mTestErrorUrl,
                            /* clickableText= */ true),
                    new TokenError(
                            AccountSelectionViewBinder.GENERIC,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.GENERIC,
                            mTestErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.INVALID_REQUEST,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.INVALID_REQUEST,
                            mTestErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.UNAUTHORIZED_CLIENT,
                            mTestErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.ACCESS_DENIED,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.ACCESS_DENIED,
                            mTestErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.TEMPORARILY_UNAVAILABLE,
                            mTestErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.SERVER_ERROR,
                            mTestEmptyErrorUrl,
                            /* clickableText= */ false),
                    new TokenError(
                            AccountSelectionViewBinder.SERVER_ERROR,
                            mTestErrorUrl,
                            /* clickableText= */ false)
                };

        for (TokenError error : mErrors) {
            ErrorProperties.Properties properties = new ErrorProperties.Properties();
            properties.mIdpForDisplay = TEST_IDP_ETLD_PLUS_ONE;
            properties.mRpForDisplay = TEST_RP_ETLD_PLUS_ONE;
            properties.mError = new IdentityCredentialTokenError(error.mCode, error.mUrl);

            AccountSelectionViewBinder.ErrorText actualError =
                    AccountSelectionViewBinder.getErrorText(
                            mContext, properties, error.mClickableText);

            assertEquals(
                    "Incorrect error summary text", error.mExpectedSummary, actualError.mSummary);

            // We use toString() here because otherwise getText() returns a
            // Spanned, which is not equal to the string we get from the resources.
            assertEquals(
                    "Incorrect error description text",
                    error.mExpectedDescription,
                    actualError.mDescription.toString());
        }
    }

    @Test
    public void testContentDescription() {
        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                buildContinueButton(mAnaAccount, mIdpMetadata, HeaderType.SIGN_IN));
        // Check that there is not a period in the content description since one will be appended.
        assertEquals(
                "Sign in bottom sheet", mBottomSheetContent.getSheetContentDescription(mContext));
    }

    @Test
    public void testMultipleIdPLogins() {
        mSheetAccountItems.addAll(
                asList(
                        buildIdpLoginItem(mIdpData, /* showIdp= */ true),
                        buildIdpLoginItem(mIdpDataWithoutIcons, /* showIdp= */ true)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        RecyclerView buttons = mContentView.findViewById(R.id.sheet_item_list);
        assertEquals("Incorrect account count", 2, buttons.getChildCount());
        View idpLogin = buttons.getChildAt(0);
        TextView title = idpLogin.findViewById(R.id.title);
        assertEquals("Use your " + mTestEtldPlusOne2 + " account", title.getText());
        assertEquals(
                "Use your " + mTestEtldPlusOne2 + " account, opens in a new tab",
                title.getContentDescription());
    }

    private RecyclerView getAccounts() {
        return mContentView.findViewById(R.id.sheet_item_list);
    }

    private TextView getAccountNameAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.title);
    }

    private TextView getAccountDescriptionAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.description);
    }

    private TextView getAccountSecondaryDescriptionAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.secondary_description);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private PropertyModel buildContinueButton(
            Account account,
            IdentityProviderMetadata idpMetadata,
            HeaderProperties.HeaderType headerType) {
        ContinueButtonProperties.Properties properties = new ContinueButtonProperties.Properties();
        properties.mAccount = account;
        properties.mIdpMetadata = idpMetadata;
        properties.mOnClickListener = mAccountCallback;
        properties.mHeaderType = headerType;
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel buildDataSharingConsentItem(String idpEtldPlusOne) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpEtldPlusOne;
        properties.mTermsOfServiceUrl = new GURL("https://www.one.com/");
        properties.mPrivacyPolicyUrl = new GURL("https://www.two.com/");
        properties.mDisclosureFields = DEFAULT_DISCLOSURE_FIELDS;

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel buildIdpSignInItem(String idpEtldPlusOne) {
        return new PropertyModel.Builder(IdpSignInProperties.ALL_KEYS)
                .with(IdpSignInProperties.IDP_FOR_DISPLAY, idpEtldPlusOne)
                .build();
    }
}
