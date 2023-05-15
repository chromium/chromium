// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
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
    private static class FakeTabCreator extends MockTabCreator {
        FakeTabCreator() {
            super(false, null);
        }

        @Override
        public Tab launchUrl(String url, @TabLaunchType int type) {
            mLastLaunchedUrl = url;
            return null;
        }

        String mLastLaunchedUrl;
    };

    private static final FakeTabCreator sTabCreator = new FakeTabCreator();

    private static final String EXAMPLE_ETLD_PLUS_ONE = "example.com";
    private static final String TEST_ETLD_PLUS_ONE_1 = "one.com";
    private static final String TEST_ETLD_PLUS_ONE_2 = "two.com";
    private static final GURL TEST_PROFILE_PIC =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1_WITH_PATH);

    private static final Account ANA = new Account("Ana", "ana@one.test", "Ana Doe", "Ana",
            TEST_PROFILE_PIC, /*hints=*/new String[0], true);
    private static final Account BOB =
            new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, /*hints=*/new String[0], false);

    private static final IdentityProviderMetadata IDP_METADATA =
            new IdentityProviderMetadata(Color.BLACK, Color.BLACK, null, null);

    private AccountSelectionComponent mAccountSelection;

    @Mock
    private AccountSelectionComponent.Delegate mMockBridge;

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
        runOnUiThreadBlocking(() -> {
            mBottomSheetController = BottomSheetControllerProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
            mAccountSelection = new AccountSelectionCoordinator(
                    mActivityTestRule.getActivity(), mBottomSheetController, mMockBridge);
        });

        mTestUrlTermsOfService =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title1.html");
        mTestUrlPrivacyPolicy =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/title2.html");
        mClientIdMetadata = new ClientIdMetadata(
                new GURL(mTestUrlTermsOfService), new GURL(mTestUrlPrivacyPolicy));
    }

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(() -> {
            mAccountSelection.showAccounts(EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2, Arrays.asList(ANA, BOB), IDP_METADATA, mClientIdMetadata,
                    false /* isAutoReauthn */, "signin" /* rpContext */);
        });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
    }

    private void testClickOnConsentLink(int linkIndex, String expectedUrl) {
        runOnUiThreadBlocking(() -> {
            mAccountSelection.showAccounts(EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2, Arrays.asList(BOB), IDP_METADATA, mClientIdMetadata,
                    false /* isAutoReauthn */, "signin" /* rpContext */);
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

        CustomTabActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class, Stage.RESUMED, () -> spans[linkIndex].onClick(null));
        CriteriaHelper.pollUiThread(() -> {
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
        BottomSheetContent otherBottomSheetContent = runOnUiThreadBlocking(() -> {
            TextView highPriorityBottomSheetContentView =
                    new TextView(mActivityTestRule.getActivity());
            highPriorityBottomSheetContentView.setText("Another bottom sheet content");
            BottomSheetContent content =
                    createTestBottomSheetContent(highPriorityBottomSheetContentView);
            mBottomSheetController.requestShowContent(content, false);
            return content;
        });
        pollUiThread(() -> getBottomSheetState() == SheetState.PEEK);
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(() -> {
            mAccountSelection.showAccounts(EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2, Arrays.asList(ANA, BOB), IDP_METADATA, mClientIdMetadata,
                    false /* isAutoReauthn */, "signin" /* rpContext */);
        });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any(), any());
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> { mBottomSheetController.hideContent(otherBottomSheetContent, false); });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
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
                return false;
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
