// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static java.util.Arrays.asList;

import android.content.res.Resources;
import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.Collections;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowGURL.class})
public class AccountSelectionViewTest {
    private static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
    private static final GURL TEST_CONFIG_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);

    private static final Account ANA =
            new Account("Ana", "ana@email.example", "Ana Doe", "Ana", TEST_PROFILE_PIC, true);
    private static final Account NO_ONE =
            new Account("", "", "No Subject", "", TEST_PROFILE_PIC, true);
    private static final Account BOB = new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, true);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private Callback<Account> mAccountCallback;

    private Resources mResources;
    private PropertyModel mModel;
    private ModelList mSheetAccountItems;
    private View mContentView;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivityScenarioRule.getScenario().onActivity(activity -> {
            mModel = new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS)
                             .build();
            mSheetAccountItems = new ModelList();
            mContentView = AccountSelectionCoordinator.setupContentView(
                    activity, mModel, mSheetAccountItems);
            activity.setContentView(mContentView);
            mResources = activity.getResources();
        });
    }

    @Test
    public void testSignInTitleDisplayedWithoutIframe() {
        mModel.set(ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                        .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals("Incorrect title",
                mResources.getString(
                        R.string.account_selection_sheet_title_explicit, "example.org", "idp.org"),
                title.getText());
        assertEquals("Incorrect subtitle", "", subtitle.getText());
    }

    @Test
    public void testSignInTitleDisplayedWithIframe() {
        mModel.set(ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                        .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "iframe-example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals("Incorrect title",
                mResources.getString(R.string.account_selection_sheet_title_explicit,
                        "iframe-example.org", "idp.org"),
                title.getText());
        assertEquals("Incorrect subtitle",
                mResources.getString(
                        R.string.account_selection_sheet_subtitle_explicit, "example.org"),
                subtitle.getText());
    }

    @Test
    public void testVerifyingTitleDisplayedExplicitSignin() {
        mModel.set(ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.VERIFY)
                        .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals("Incorrect title", mResources.getString(R.string.verify_sheet_title),
                title.getText());
        assertEquals("Incorrect subtitle", "", subtitle.getText());
    }

    @Test
    public void testVerifyingTitleDisplayedAutoReauthn() {
        mModel.set(ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.VERIFY_AUTO_REAUTHN)
                        .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals("Incorrect title",
                mResources.getString(R.string.verify_sheet_title_auto_reauthn), title.getText());
        assertEquals("Incorrect subtitle", "", subtitle.getText());
    }

    @Test
    public void testAccountsChangedByModel() {
        mSheetAccountItems.addAll(
                asList(buildAccountItem(ANA), buildAccountItem(NO_ONE), buildAccountItem(BOB)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        assertEquals("Incorrect account count", 3, getAccounts().getChildCount());
        assertEquals("Incorrect name", ANA.getName(), getAccountNameAt(0).getText());
        assertEquals("Incorrect email", ANA.getEmail(), getAccountEmailAt(0).getText());
        assertEquals("Incorrect name", NO_ONE.getName(), getAccountNameAt(1).getText());
        assertEquals("Incorrect email", NO_ONE.getEmail(), getAccountEmailAt(1).getText());
        assertEquals("Incorrect name", BOB.getName(), getAccountNameAt(2).getText());
        assertEquals("Incorrect email", BOB.getEmail(), getAccountEmailAt(2).getText());
    }

    @Test
    public void testAccountsAreClickable() {
        mSheetAccountItems.addAll(Collections.singletonList(buildAccountItem(ANA)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());

        assertNotNull(getAccounts().getChildAt(0));

        getAccounts().getChildAt(0).performClick();

        waitForEvent(mAccountCallback).onResult(eq(ANA));
    }

    @Test
    public void testSingleAccountHasClickableButton() {
        // Create an account with no callback to ensure the button callback
        // is the one that gets invoked.
        mSheetAccountItems.add(
                new MVCListAdapter.ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                        new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                .with(AccountProperties.ACCOUNT, ANA)
                                .with(AccountProperties.ON_CLICK_LISTENER, null)
                                .build()));
        ShadowLooper.shadowMainLooper().idle();

        mModel.set(ItemProperties.CONTINUE_BUTTON, buildContinueButton(ANA, null));
        assertEquals(View.VISIBLE, mContentView.getVisibility());

        assertNotNull(getAccounts().getChildAt(0));

        View continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);
        assertTrue(continueButton.isShown());
        continueButton.performClick();

        waitForEvent(mAccountCallback).onResult(eq(ANA));
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
                mResources.getString(R.string.account_selection_data_sharing_consent, "idp.org");
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
    public void testContinueButtonBranding() {
        final int expectedTextColor = Color.BLUE;
        IdentityProviderMetadata idpMetadata = new IdentityProviderMetadata(expectedTextColor,
                /*brandBackgroundColor*/ Color.GREEN, "https://icon-url.example", TEST_CONFIG_URL);

        mModel.set(ItemProperties.CONTINUE_BUTTON, buildContinueButton(ANA, idpMetadata));

        assertEquals(View.VISIBLE, mContentView.getVisibility());

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

    private PropertyModel buildDataSharingConsentItem(String idpEtldPlusOne) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpEtldPlusOne;
        properties.mTermsOfServiceUrl = new GURL("https://www.one.com/");
        properties.mPrivacyPolicyUrl = new GURL("https://www.two.com/");

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }
}
