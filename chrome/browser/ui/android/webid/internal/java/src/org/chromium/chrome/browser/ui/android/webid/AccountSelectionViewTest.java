// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import static java.util.Arrays.asList;

import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AutoSignInCancelButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountSelectionViewTest {
    private static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    private static final Account ANA =
            new Account("Ana", "ana@email.example", "Ana Doe", "Ana", TEST_PROFILE_PIC, true);
    private static final Account NO_ONE =
            new Account("", "", "No Subject", "", TEST_PROFILE_PIC, true);
    private static final Account BOB = new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, true);

    @Mock
    private Callback<Account> mAccountCallback;
    private Runnable mAutoSignInCancelCallback;

    private BlankUiTestActivity mActivity;
    private PropertyModel mModel;
    private ModelList mSheetAccountItems;
    private View mContentView;
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS)
                             .build();
            mSheetAccountItems = new ModelList();
            mContentView = AccountSelectionCoordinator.setupContentView(
                    mActivity, mModel, mSheetAccountItems);
            mActivity.setContentView(mContentView);
        });
    }

    @Test
    @MediumTest
    public void testSignInTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .build());
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title",
                mActivity.getString(
                        R.string.account_selection_sheet_title_explicit, "example.org", "idp.org"),
                title.getText());
    }

    @Test
    @MediumTest
    public void testVerifyingTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.VERIFY)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .build());
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title", mActivity.getString(R.string.verify_sheet_title),
                title.getText());
    }

    @Test
    @MediumTest
    public void testAccountsChangedByModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetAccountItems.addAll(
                    asList(buildAccountItem(ANA), buildAccountItem(NO_ONE), buildAccountItem(BOB)));
        });

        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        assertEquals("Incorrect account count", 3, getAccounts().getChildCount());
        assertEquals("Incorrect name", ANA.getName(), getAccountNameAt(0).getText());
        assertEquals("Incorrect email", ANA.getEmail(), getAccountEmailAt(0).getText());
        assertEquals("Incorrect name", NO_ONE.getName(), getAccountNameAt(1).getText());
        assertEquals("Incorrect email", NO_ONE.getEmail(), getAccountEmailAt(1).getText());
        assertEquals("Incorrect name", BOB.getName(), getAccountNameAt(2).getText());
        assertEquals("Incorrect email", BOB.getEmail(), getAccountEmailAt(2).getText());
    }

    @Test
    @MediumTest
    public void testAccountsAreClickable() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetAccountItems.addAll(Collections.singletonList(buildAccountItem(ANA)));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));

        TouchCommon.singleClickView(getAccounts().getChildAt(0));

        waitForEvent(mAccountCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testSingleAccountHasClickableButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create an account with no callback to ensure the button callback
            // is the one that gets invoked.
            mSheetAccountItems.add(
                    new MVCListAdapter.ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                            new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                    .with(AccountProperties.ACCOUNT, ANA)
                                    .with(AccountProperties.ON_CLICK_LISTENER, null)
                                    .build()));

            mModel.set(ItemProperties.CONTINUE_BUTTON, buildContinueButton(ANA, null));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));

        View continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);
        assertTrue(continueButton.isShown());
        TouchCommon.singleClickView(continueButton);

        waitForEvent(mAccountCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testAutoSignInCancellButtonDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create an account with no callback to ensure the button callback
            // is the one that gets invoked.
            mSheetAccountItems.add(
                    new MVCListAdapter.ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                            new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                    .with(AccountProperties.ACCOUNT, ANA)
                                    .with(AccountProperties.ON_CLICK_LISTENER, null)
                                    .build()));

            mModel.set(ItemProperties.AUTO_SIGN_IN_CANCEL_BUTTON, buildCancelButton());
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));
        assertTrue(mContentView.findViewById(R.id.auto_sign_in_cancel_btn).isShown());
    }

    @Test
    @MediumTest
    public void testHeaderDisplayedForAutoSignIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.AUTO_SIGN_IN)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .build());
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title",
                mActivity.getString(
                        R.string.account_selection_sheet_title_auto, "example.org", "idp.org"),
                title.getText());
    }

    @Test
    @MediumTest
    public void testDataSharingConsentDisplayed() {
        final String idpEtldPlusOne = "idp.org";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ItemProperties.DATA_SHARING_CONSENT,
                    buildDataSharingConsentItem(idpEtldPlusOne));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView consent = mContentView.findViewById(R.id.user_data_sharing_consent);
        assertTrue(consent.isShown());
        String expectedSharingConsentText =
                mActivity.getString(R.string.account_selection_data_sharing_consent, "idp.org");
        expectedSharingConsentText = expectedSharingConsentText.replaceAll("<[^>]*>", "");
        // We use toString() here because otherwise getText() returns a
        // Spanned, which is not equal to the string we get from the resources.
        assertEquals("Incorrect data sharing consent text", expectedSharingConsentText,
                consent.getText().toString());
        Spanned spannedString = (Spanned) consent.getText();
        ClickableSpan[] spans =
                spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
        assertEquals("Expected two clickable links", 2, spans.length);
    }

    /**
     * Tests that the brand foreground and the brand icon are used in the "Continue" button.
     */
    @Test
    @MediumTest
    public void testContinueButtonBranding() {
        final int expectedTextColor = Color.BLUE;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityProviderMetadata idpMetadata = new IdentityProviderMetadata(expectedTextColor,
                    /*brandBackgroundColor*/ Color.GREEN, "https://icon-url.example");

            mModel.set(ItemProperties.CONTINUE_BUTTON, buildContinueButton(ANA, idpMetadata));
        });

        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        TextView continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);

        assertEquals(expectedTextColor, continueButton.getTextColors().getDefaultColor());
    }

    private RecyclerView getAccounts() {
        return mContentView.findViewById(R.id.sheet_item_list);
    }

    private TextView getAccountNameAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.title);
    }

    private TextView getAccountEmailAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.description);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private MVCListAdapter.ListItem buildAccountItem(Account account) {
        return new MVCListAdapter.ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(AccountProperties.ON_CLICK_LISTENER, mAccountCallback)
                        .build());
    }

    private PropertyModel buildContinueButton(
            Account account, IdentityProviderMetadata idpMetadata) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                        .with(ContinueButtonProperties.ACCOUNT, account)
                        .with(ContinueButtonProperties.ON_CLICK_LISTENER, mAccountCallback);
        if (idpMetadata != null) {
            modelBuilder.with(ContinueButtonProperties.IDP_METADATA, idpMetadata);
        }

        return modelBuilder.build();
    }

    private PropertyModel buildCancelButton() {
        return new PropertyModel.Builder(AutoSignInCancelButtonProperties.ALL_KEYS)
                .with(AutoSignInCancelButtonProperties.ON_CLICK_LISTENER, mAutoSignInCancelCallback)
                .build();
    }

    private PropertyModel buildDataSharingConsentItem(String idpEtldPlusOne) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpEtldPlusOne;
        properties.mTermsOfServiceUrl = "https://rp.com/tos";
        properties.mPrivacyPolicyUrl = "https://rp.com/privacy";

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }
}
