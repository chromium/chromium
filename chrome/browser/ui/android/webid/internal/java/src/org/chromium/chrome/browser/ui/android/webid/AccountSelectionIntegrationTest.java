// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/**
 * Integration tests for the Account Selection component check that the calls to the Account
 * Selection API end up rendering a View.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountSelectionIntegrationTest {
    private static final String EXAMPLE_ETLD_PLUS_ONE = "example.com";
    private static final String TEST_ETLD_PLUS_ONE_1 = "one.com";
    private static final String TEST_ETLD_PLUS_ONE_2 = "two.com";
    private static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.URL_1_WITH_PATH;
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    private static final Account ANA =
            new Account("Ana", "ana@one.test", "Ana Doe", "Ana", TEST_PROFILE_PIC, null, true);
    private static final Account BOB =
            new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, null, false);

    private static final IdentityProviderMetadata IDP_METADATA =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconUrl= */ null,
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* supports_add_account= */ false);
    private static final IdentityProviderMetadata IDP_METADATA_WITH_ADD_ACCOUNT =
            new IdentityProviderMetadata(
                    /* brandTextColor= */ Color.WHITE,
                    /* brandBackgroundColor= */ Color.BLACK,
                    /* brandIconUrl= */ null,
                    /* configUrl= */ null,
                    /* loginUrl= */ null,
                    /* supports_add_account= */ true);

    private static final String TEST_ERROR_CODE = "invalid_request";
    private static final IdentityCredentialTokenError TOKEN_ERROR =
            new IdentityCredentialTokenError(TEST_ERROR_CODE, TEST_URL);

    private AccountSelectionCoordinator mAccountSelection;

    @Mock private AccountSelectionComponent.Delegate mMockBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mBottomSheetController;

    private String mTestUrlTermsOfService;
    private String mTestUrlPrivacyPolicy;
    private ClientIdMetadata mClientIdMetadata;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
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
                                    mMockBridge);
                });

        mTestUrlTermsOfService =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title1.html");
        mTestUrlPrivacyPolicy =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title2.html");
        mClientIdMetadata =
                new ClientIdMetadata(
                        new GURL(mTestUrlTermsOfService), new GURL(mTestUrlPrivacyPolicy));
    }

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(ANA, BOB),
                            IDP_METADATA,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(ANA, BOB),
                            IDP_METADATA,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    private void testClickOnConsentLink(int linkIndex, String expectedUrl) {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(BOB),
                            IDP_METADATA,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);
        TextView consent = contentView.findViewById(R.id.user_data_sharing_consent);
        if (consent == null) {
            throw new NoMatchingViewException.Builder()
                    .includeViewHierarchy(true)
                    .withRootView(contentView)
                    .build();
        }
        assertTrue(consent.getText() instanceof Spanned);
        Spanned spannedString = (Spanned) consent.getText();
        ClickableSpan[] spans =
                spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
        assertEquals("Expected two clickable links", 2, spans.length);

        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> spans[linkIndex].onClick(null));
        CriteriaHelper.pollUiThread(
                () -> {
                    return activity.getActivityTab() != null
                            && activity.getActivityTab().getUrl().getSpec().equals(expectedUrl);
                });
    }

    @Test
    @MediumTest
    public void testClickPrivacyPolicyLink() {
        testClickOnConsentLink(0, mTestUrlPrivacyPolicy);
    }

    @Test
    @MediumTest
    public void testClickTermsOfServiceLink() {
        testClickOnConsentLink(1, mTestUrlTermsOfService);
    }

    @Test
    @MediumTest
    @SuppressLint("SetTextI18n")
    public void testDismissedIfUnableToShow() throws Exception {
        BottomSheetContent otherBottomSheetContent =
                runOnUiThreadBlocking(
                        () -> {
                            TextView highPriorityBottomSheetContentView =
                                    new TextView(mActivityTestRule.getActivity());
                            highPriorityBottomSheetContentView.setText(
                                    "Another bottom sheet content");
                            BottomSheetContent content =
                                    createTestBottomSheetContent(
                                            highPriorityBottomSheetContentView);
                            mBottomSheetController.requestShowContent(content, false);
                            return content;
                        });
        pollUiThread(() -> getBottomSheetState() == SheetState.PEEK);
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(ANA, BOB),
                            IDP_METADATA,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(otherBottomSheetContent, false);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
    }

    @Test
    @MediumTest
    public void testFailureDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showFailureDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            /* rpContext= */ "signin");
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testFailureDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showFailureDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            /* rpContext= */ "signin");
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testShowAndCloseModalDialog() {
        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> mAccountSelection.showModalDialog(TEST_URL));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getActivityTab(), Matchers.notNullValue());
                    Criteria.checkThat(activity.getActivityTab().getUrl(), is(TEST_URL));
                    Criteria.checkThat(activity.getIntent(), Matchers.notNullValue());
                    Criteria.checkThat(
                            activity.getIntent().getIntExtra(IntentHandler.EXTRA_FEDCM_ID, -1),
                            Matchers.not(-1));
                });

        ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class,
                Stage.DESTROYED,
                () -> {
                    BottomSheetController customTabController =
                            BottomSheetControllerProvider.from(activity.getWindowAndroid());
                    AccountSelectionComponent customTabComponent =
                            new AccountSelectionCoordinator(
                                    activity.getActivityTab(),
                                    activity.getWindowAndroid(),
                                    customTabController,
                                    mMockBridge);
                    customTabComponent.closeModalDialog();
                });
    }

    @Test
    @MediumTest
    public void testShowModalDialogAndFinish() {
        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.RESUMED,
                        () -> mAccountSelection.showModalDialog(TEST_URL));
        runOnUiThreadBlocking(
                () -> {
                    activity.finish();
                });
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testIncorrectCloseModalDialog() {
        // closeModalDialog() on the mAccountSelection should do nothing.
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.closeModalDialog();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().isDestroyed(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    public void testErrorDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            /* rpContext= */ "signin",
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testErrorDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            /* rpContext= */ "signin",
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountForNewUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        // This should be the "multi-account chooser", so clicking an account should go
        // to the privacy policy/TOS screen.
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_continue_btn)).check(matches(withText("Add Account")));

        // Click the first account
        runOnUiThreadBlocking(
                () -> {
                    ((RecyclerView) contentView.findViewById(R.id.sheet_item_list))
                            .getChildAt(0)
                            .performClick();
                });

        // Sheet should still be open
        assertNotEquals(BottomSheetController.SheetState.HIDDEN, getBottomSheetState());
        onView(withId(R.id.account_selection_continue_btn))
                .check(matches(withText("Continue as Bob")));

        // Make sure we now show the pp/tos block.
        TextView consent = contentView.findViewById(R.id.user_data_sharing_consent);
        if (consent == null) {
            throw new NoMatchingViewException.Builder()
                    .includeViewHierarchy(true)
                    .withRootView(contentView)
                    .build();
        }

        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_continue_btn).performClick();
                });

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountReturningUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(ANA),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_continue_btn)).check(matches(withText("Add Account")));

        // Click the first account
        runOnUiThreadBlocking(
                () -> {
                    ((RecyclerView) contentView.findViewById(R.id.sheet_item_list))
                            .getChildAt(0)
                            .performClick();
                });

        // Because this is a returning account, we should immediately sign in now.
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_1,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(BOB),
                            IDP_METADATA_WITH_ADD_ACCOUNT,
                            mClientIdMetadata,
                            /* isAutoReauthn= */ false,
                            /* rpContext= */ "signin",
                            /* requestPermission= */ true);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_continue_btn)).check(matches(withText("Add Account")));

        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mAccountSelection.showAccounts(
                                        EXAMPLE_ETLD_PLUS_ONE,
                                        TEST_ETLD_PLUS_ONE_1,
                                        TEST_ETLD_PLUS_ONE_2,
                                        Arrays.asList(ANA),
                                        IDP_METADATA_WITH_ADD_ACCOUNT,
                                        mClientIdMetadata,
                                        /* isAutoReauthn= */ false,
                                        /* rpContext= */ "signin",
                                        /* requestPermission= */ true);
                                mAccountSelection.getMediator().setComponentShowTime(-1000);
                                return null;
                            }
                        })
                .when(mMockBridge)
                .onLoginToIdP(any(), any());

        // Click Add Account.
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_continue_btn).performClick();
                });

        // Make sure that the Ana account is now displayed.
        onView(withText("Ana Doe")).check(matches(isDisplayed()));

        // Because of how we implemented onLogInToIdP, we should be back to
        // account chooser here. Click the account.
        runOnUiThreadBlocking(
                () -> {
                    ((RecyclerView) contentView.findViewById(R.id.sheet_item_list))
                            .getChildAt(0)
                            .performClick();
                });

        // Because this is a returning account, we should immediately sign in now.
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any(), any());
    }

    public static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private BottomSheetContent createTestBottomSheetContent(View contentView) {
        return new BottomSheetContent() {
            @Override
            public View getContentView() {
                return contentView;
            }

            @Nullable
            @Override
            public View getToolbarView() {
                return null;
            }

            @Override
            public int getVerticalScrollOffset() {
                return 0;
            }

            @Override
            public void destroy() {}

            @Override
            public int getPriority() {
                return ContentPriority.HIGH;
            }

            @Override
            public boolean swipeToDismissEnabled() {
                return true;
            }

            @Override
            public int getSheetContentDescriptionStringId() {
                return 0;
            }

            @Override
            public int getSheetHalfHeightAccessibilityStringId() {
                return 0;
            }

            @Override
            public int getSheetFullHeightAccessibilityStringId() {
                return 0;
            }

            @Override
            public int getSheetClosedAccessibilityStringId() {
                return 0;
            }
        };
    }
}
