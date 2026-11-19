// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import static java.util.Arrays.asList;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.CallbackUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet. This class is parameterized to run all tests for each RP mode, with {@code
 * FedCmPermissionUiWithContainer} and {@code FedCmIdpLinks} each disabled and enabled.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class AccountSelectionViewTest extends AccountSelectionJUnitTestBase {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {
                    {RpMode.PASSIVE, false, false},
                    {RpMode.PASSIVE, true, false},
                    {RpMode.PASSIVE, false, true},
                    {RpMode.PASSIVE, true, true},
                    {RpMode.ACTIVE, false, false},
                    {RpMode.ACTIVE, true, false},
                    {RpMode.ACTIVE, false, true},
                    {RpMode.ACTIVE, true, true}
                });
    }

    /** Whether {@code FedCmPermissionUiWithContainer} is enabled. */
    @Parameter(1)
    public boolean mPermissionUiWithContainer;

    /** Whether {@code FedCmIdpLinks} is enabled. */
    @Parameter(2)
    public boolean mIdpLinks;

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

        private String appendExtraDescription(String code, GURL url, boolean clickableText) {
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

    @Before
    @Override
    public void setUp() {
        FeatureOverrides.overrideFlag(
                ChromeFeatureList.FED_CM_PERMISSION_UI_WITH_CONTAINER, mPermissionUiWithContainer);
        FeatureOverrides.overrideFlag(ChromeFeatureList.FED_CM_IDP_LINKS, mIdpLinks);
        super.setUp();
    }

    @Test
    public void testAccountsChangedByModel() {
        mSheetAccountItems.addAll(
                asList(
                        buildAccountItem(mAnaAccount, /* showIdp= */ false),
                        buildAccountItem(mNoOneAccount, /* showIdp= */ false),
                        buildAccountItem(mBobAccount, /* showIdp= */ false),
                        buildAccountItem(mNicolasAccount, /* showIdp= */ false)));
        RobolectricUtil.runAllBackgroundAndUi();

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
        RobolectricUtil.runAllBackgroundAndUi();

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
        RobolectricUtil.runAllBackgroundAndUi();

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
        RobolectricUtil.runAllBackgroundAndUi();

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

        // Only the active mode sheet shows the text next to an icon in a container, and only with
        // `FedCmPermissionUiWithContainer` or `FedCmIdpLinks` enabled.
        View container = mContentView.findViewById(R.id.user_data_sharing_consent_container);
        if (mRpMode == RpMode.ACTIVE && (mPermissionUiWithContainer || mIdpLinks)) {
            View row = container.findViewById(R.id.user_data_sharing_consent_row);
            assertEquals(row, consent.getParent());
            assertEquals(
                    R.drawable.account_selection_active_mode_consent_container_background,
                    shadowOf(row.getBackground()).getCreatedFromResId());
            assertTrue(row.findViewById(R.id.user_data_sharing_consent_icon).isShown());
            // The consent item has no identity provider links, so their row is hidden.
            assertFalse(container.findViewById(R.id.idp_links_row).isShown());
        } else {
            assertNull(container);
        }

        // The container, if any, is hidden along with the text.
        mModel.set(ItemProperties.DATA_SHARING_CONSENT, null);
        assertFalse(consent.isShown());
        if (container != null) {
            assertFalse(container.isShown());
        }
    }

    /**
     * Tests that the active mode container shows the identity provider's links in a second row
     * below the disclosure text, for whichever of its account management page, privacy policy and
     * terms of service it has. Other sheets have no such row.
     */
    @Test
    public void testIdpLinksDisplayed() {
        if (mRpMode == RpMode.ACTIVE && (mPermissionUiWithContainer || mIdpLinks)) {
            GURL empty = GURL.emptyGURL();
            checkIdpLinksRow(
                    mTestIdpAccountManagementUrl,
                    mTestIdpPrivacyPolicyUrl,
                    mTestIdpTermsOfServiceUrl,
                    R.string.account_selection_idp_links);
            checkIdpLinksRow(
                    mTestIdpAccountManagementUrl,
                    mTestIdpPrivacyPolicyUrl,
                    empty,
                    R.string.account_selection_idp_links_no_tos);
            checkIdpLinksRow(
                    mTestIdpAccountManagementUrl,
                    empty,
                    mTestIdpTermsOfServiceUrl,
                    R.string.account_selection_idp_links_no_pp);
            checkIdpLinksRow(
                    mTestIdpAccountManagementUrl,
                    empty,
                    empty,
                    R.string.account_selection_idp_links_no_pp_or_tos);
            checkIdpLinksRow(
                    empty,
                    mTestIdpPrivacyPolicyUrl,
                    mTestIdpTermsOfServiceUrl,
                    R.string.account_selection_idp_links_no_account_management);
            checkIdpLinksRow(
                    empty,
                    mTestIdpPrivacyPolicyUrl,
                    empty,
                    R.string.account_selection_idp_links_no_account_management_or_tos);
            checkIdpLinksRow(
                    empty,
                    empty,
                    mTestIdpTermsOfServiceUrl,
                    R.string.account_selection_idp_links_no_account_management_or_pp);

            // Without any links, the row is hidden, and all the corners of the disclosure text's
            // row are rounded again.
            mModel.set(
                    ItemProperties.DATA_SHARING_CONSENT,
                    buildDataSharingConsentItem(mTestEtldPlusOne2));
            assertFalse(mContentView.findViewById(R.id.idp_links_row).isShown());
            assertEquals(
                    R.drawable.account_selection_active_mode_consent_container_background,
                    shadowOf(
                                    mContentView
                                            .findViewById(R.id.user_data_sharing_consent_row)
                                            .getBackground())
                            .getCreatedFromResId());
        } else {
            // Only the active mode container has a row for the identity provider's links.
            mModel.set(
                    ItemProperties.DATA_SHARING_CONSENT,
                    buildDataSharingConsentItem(
                            mTestEtldPlusOne2,
                            mTestIdpAccountManagementUrl,
                            mTestIdpPrivacyPolicyUrl,
                            mTestIdpTermsOfServiceUrl,
                            (context, url) -> {}));
            assertNull(mContentView.findViewById(R.id.idp_links_row));
        }
    }

    /**
     * Shows the disclosure text with the given identity provider links, and checks that their row
     * has the `expectedTextId` text, with a link to each non-empty URL in order.
     */
    private void checkIdpLinksRow(
            GURL accountManagementUrl,
            GURL privacyPolicyUrl,
            GURL termsOfServiceUrl,
            @StringRes int expectedTextId) {
        List<GURL> clickedUrls = new ArrayList<>();
        mModel.set(
                ItemProperties.DATA_SHARING_CONSENT,
                buildDataSharingConsentItem(
                        mTestEtldPlusOne2,
                        accountManagementUrl,
                        privacyPolicyUrl,
                        termsOfServiceUrl,
                        (context, url) -> clickedUrls.add(url)));

        View row = mContentView.findViewById(R.id.idp_links_row);
        assertTrue(row.isShown());
        assertTrue(row.findViewById(R.id.idp_links_icon).isShown());
        assertEquals(
                R.drawable.account_selection_active_mode_consent_container_bottom_background,
                shadowOf(row.getBackground()).getCreatedFromResId());
        // Only the top corners of the disclosure text's row are rounded.
        assertEquals(
                R.drawable.account_selection_active_mode_consent_container_top_background,
                shadowOf(
                                mContentView
                                        .findViewById(R.id.user_data_sharing_consent_row)
                                        .getBackground())
                        .getCreatedFromResId());

        TextView idpLinks = row.findViewById(R.id.idp_links);
        assertEquals(
                mResources
                        .getString(expectedTextId, mTestEtldPlusOne2)
                        .replaceAll(LINK_TAG_REGEX, ""),
                idpLinks.getText().toString());

        // Clicking each link, in order, passes its URL to the click callback.
        Spanned text = (Spanned) idpLinks.getText();
        ClickableSpan[] spans = text.getSpans(0, text.length(), ClickableSpan.class);
        Arrays.sort(spans, (a, b) -> text.getSpanStart(a) - text.getSpanStart(b));
        for (ClickableSpan span : spans) {
            span.onClick(idpLinks);
        }
        List<GURL> expectedUrls = new ArrayList<>();
        for (GURL url : Arrays.asList(accountManagementUrl, privacyPolicyUrl, termsOfServiceUrl)) {
            if (!url.isEmpty()) {
                expectedUrls.add(url);
            }
        }
        assertEquals(expectedUrls, clickedUrls);
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
                        /* accountManagementUrl= */ GURL.emptyGURL(),
                        /* privacyPolicyUrl= */ GURL.emptyGURL(),
                        /* termsOfServiceUrl= */ GURL.emptyGURL(),
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
        final TokenError[] errors =
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

        for (TokenError error : errors) {
            ErrorProperties.Properties properties =
                    new ErrorProperties.Properties(
                            TEST_IDP_ETLD_PLUS_ONE,
                            TEST_RP_ETLD_PLUS_ONE,
                            new IdentityCredentialTokenError(error.mCode, error.mUrl),
                            error.mUrl.isEmpty() ? null : CallbackUtils.emptyRunnable());

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
        RobolectricUtil.runAllBackgroundAndUi();

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
            Account account, IdentityProviderMetadata idpMetadata, @HeaderType int headerType) {
        ContinueButtonProperties.Properties properties =
                new ContinueButtonProperties.Properties(
                        account,
                        idpMetadata,
                        mAccountCallback,
                        headerType,
                        /* setFocusViewCallback= */ null);
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel buildDataSharingConsentItem(String idpEtldPlusOne) {
        return buildDataSharingConsentItem(
                idpEtldPlusOne,
                GURL.emptyGURL(),
                GURL.emptyGURL(),
                GURL.emptyGURL(),
                (context, url) -> {});
    }

    private PropertyModel buildDataSharingConsentItem(
            String idpEtldPlusOne,
            GURL idpAccountManagementUrl,
            GURL idpPrivacyPolicyUrl,
            GURL idpTermsOfServiceUrl,
            BiConsumer<Context, GURL> idpLinkClickCallback) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties(
                        idpEtldPlusOne,
                        new GURL("https://www.one.com/"),
                        new GURL("https://www.two.com/"),
                        context -> {},
                        context -> {},
                        idpAccountManagementUrl,
                        idpPrivacyPolicyUrl,
                        idpTermsOfServiceUrl,
                        idpLinkClickCallback,
                        /* setFocusViewCallback= */ null,
                        DEFAULT_DISCLOSURE_FIELDS);

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel buildIdpSignInItem(String idpEtldPlusOne) {
        return new PropertyModel.Builder(IdpSignInProperties.ALL_KEYS)
                .with(IdpSignInProperties.IDP_FOR_DISPLAY, idpEtldPlusOne)
                .build();
    }

    @Test
    public void testIframeDisplayed() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "rp.com")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "iframe.com")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.com")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, mRpMode)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        String expectedTitle =
                mResources.getString(
                        R.string.account_selection_sheet_title_explicit_signin,
                        "iframe.com",
                        "idp.com");

        assertEquals("Incorrect title", expectedTitle, title.getText().toString());
        assertEquals(
                "Incorrect subtitle",
                "rp.com embeds content from iframe.com",
                subtitle.getText().toString());
    }

    @Test
    public void testIsValidBrandIcon() {
        int minSize = AccountSelectionBridge.getBrandIconMinimumSize(mRpMode);
        assertTrue(minSize > 1);

        assertFalse(
                AccountSelectionViewBinder.isValidBrandIcon(
                        /* bitmap= */ null, mRpMode, /* shouldCircleCrop= */ true));
        assertFalse(
                AccountSelectionViewBinder.isValidBrandIcon(
                        /* bitmap= */ null, mRpMode, /* shouldCircleCrop= */ false));

        Bitmap nonSquare = Bitmap.createBitmap(minSize, minSize + 1, Bitmap.Config.ARGB_8888);
        assertFalse(
                AccountSelectionViewBinder.isValidBrandIcon(
                        nonSquare, mRpMode, /* shouldCircleCrop= */ true));
        assertFalse(
                AccountSelectionViewBinder.isValidBrandIcon(
                        nonSquare, mRpMode, /* shouldCircleCrop= */ false));

        Bitmap tooSmall = Bitmap.createBitmap(minSize - 1, minSize - 1, Bitmap.Config.ARGB_8888);
        assertFalse(
                AccountSelectionViewBinder.isValidBrandIcon(
                        tooSmall, mRpMode, /* shouldCircleCrop= */ true));
        assertTrue(
                AccountSelectionViewBinder.isValidBrandIcon(
                        tooSmall, mRpMode, /* shouldCircleCrop= */ false));

        Bitmap validSquare = Bitmap.createBitmap(minSize, minSize, Bitmap.Config.ARGB_8888);
        assertTrue(
                AccountSelectionViewBinder.isValidBrandIcon(
                        validSquare, mRpMode, /* shouldCircleCrop= */ true));
        assertTrue(
                AccountSelectionViewBinder.isValidBrandIcon(
                        validSquare, mRpMode, /* shouldCircleCrop= */ false));
    }
}
