// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.widget.Button;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.ViewUtils;

/** Tests for the standalone history sync consent dialog */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This test relies on native initialization")
public class HistorySyncTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private static final @SigninAccessPoint int SIGNIN_ACCESS_POINT = SigninAccessPoint.UNKNOWN;

    @Mock private SyncService mSyncServiceMock;
    @Mock private HistorySyncCoordinator.HistorySyncDelegate mHistorySyncDelegateMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;

    private HistorySyncCoordinator mHistorySyncCoordinator;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        when(mHistorySyncDelegateMock.isLargeScreen()).thenReturn(false);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
    }

    @After
    public void tearDown() {
        mSigninTestRule.forceSignOut();
    }

    @Test
    @MediumTest
    public void testHistorySyncLayout() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Started", SIGNIN_ACCESS_POINT);

        buildHistorySyncCoordinator();

        histogramWatcher.assertExpected();
        onView(withId(R.id.history_sync_title)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_account_image)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withText(R.string.history_sync_primary_action)).check(matches(isDisplayed()));
        onView(withText(R.string.history_sync_secondary_action)).check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.history_sync_footer),
                                withText(R.string.history_sync_footer_without_email)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFooterStringWithEmail() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        String expectedFooter =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.history_sync_footer_with_email,
                                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN).getEmail());

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        onView(allOf(withId(R.id.history_sync_footer), withText(expectedFooter)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWithNonMinorModeAccount() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.HistorySyncOptIn.Completed", SIGNIN_ACCESS_POINT)
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SigninMetricsUtils.SyncButtonClicked
                                        .HISTORY_SYNC_OPT_IN_NOT_EQUAL_WEIGHTED)
                        .build();

        onView(withText(R.string.history_sync_primary_action)).perform(click());

        histogramWatcher.assertExpected();
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.HISTORY, true);
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.TABS, true);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
        verify(mHistorySyncHelperMock).clearHistorySyncDeclinedPrefs();
    }

    @Test
    @MediumTest
    public void testNegativeButtonNonMinorModeAccount() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.HistorySyncOptIn.Declined", SIGNIN_ACCESS_POINT)
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SigninMetricsUtils.SyncButtonClicked
                                        .HISTORY_SYNC_CANCEL_NOT_EQUAL_WEIGHTED)
                        .build();

        onView(withText(R.string.history_sync_secondary_action)).perform(click());

        histogramWatcher.assertExpected();
        verifyNoInteractions(mSyncServiceMock);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWithMinorModeAccount() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.HistorySyncOptIn.Completed", SIGNIN_ACCESS_POINT)
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SigninMetricsUtils.SyncButtonClicked
                                        .HISTORY_SYNC_OPT_IN_EQUAL_WEIGHTED)
                        .build();

        onView(withText(R.string.history_sync_primary_action)).perform(click());

        histogramWatcher.assertExpected();
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.HISTORY, true);
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.TABS, true);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
    }

    @Test
    @MediumTest
    public void testNegativeButtonWithMinorModeAccount() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.HistorySyncOptIn.Declined", SIGNIN_ACCESS_POINT)
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SigninMetricsUtils.SyncButtonClicked
                                        .HISTORY_SYNC_CANCEL_EQUAL_WEIGHTED)
                        .build();

        onView(withText(R.string.history_sync_secondary_action)).perform(click());

        histogramWatcher.assertExpected();
        verifyNoInteractions(mSyncServiceMock);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mHistorySyncHelperMock).recordHistorySyncDeclinedPrefs();
    }

    @Test
    @MediumTest
    public void testNegativeButton_shouldSignOutOnDecline() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ false, /* shouldSignOutOnDecline= */ true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Declined", SIGNIN_ACCESS_POINT);

        onView(withText(R.string.history_sync_secondary_action)).perform(click());

        histogramWatcher.assertExpected();
        verifyNoInteractions(mSyncServiceMock);
        verify(mHistorySyncDelegateMock, atLeastOnce()).dismissHistorySync();
        CriteriaHelper.pollUiThread(
                () -> mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN) == null);
        verify(mHistorySyncHelperMock).recordHistorySyncDeclinedPrefs();
    }

    @Test
    @MediumTest
    public void testOnSignedOut() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Aborted", SIGNIN_ACCESS_POINT);

        mSigninTestRule.signOut();
        CriteriaHelper.pollUiThread(
                () -> mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN) == null);

        histogramWatcher.assertExpected();
        verify(mHistorySyncDelegateMock).dismissHistorySync();
    }

    @Test
    @MediumTest
    public void testButtonsEquallyWeightedWithMinorAccount_portraitMode() {
        Activity historySyncActivity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(
                historySyncActivity, Configuration.ORIENTATION_PORTRAIT);
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton =
                            historySyncActivity.findViewById(R.id.button_primary_minor_mode);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary_minor_mode);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());

                    Assert.assertEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }

    @Test
    @MediumTest
    public void testButtonsEquallyWeightedWithMinorAccount_landscapeMode() {
        Activity historySyncActivity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(
                historySyncActivity, Configuration.ORIENTATION_LANDSCAPE);
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }

    @Test
    @MediumTest
    public void testButtonsUnequallyWeightedWithNonMinorAccount_portraitMode() {
        Activity historySyncActivity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(
                historySyncActivity, Configuration.ORIENTATION_PORTRAIT);
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertNotEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }

    @Test
    @MediumTest
    public void testButtonsUnequallyWeightedWithNonMinorAccount_landscapeMode() {
        Activity historySyncActivity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(
                historySyncActivity, Configuration.ORIENTATION_LANDSCAPE);
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertNotEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testButtonsEquallyWeightedWithMinorAccount_CapabilityArrivesBeforeDeadline() {
        MinorModeHelper.disableTimeoutForTesting();
        Activity historySyncActivity = mActivityTestRule.getActivity();
        // Account Capabilities are intentionally empty.
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);
        mSigninTestRule.waitForSeeding();

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        // Buttons will not be visible before capability/deadline is reached.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button primaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_primary_minor_mode);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Button secondaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_secondary_minor_mode);

                    Assert.assertEquals(View.GONE, primaryButton.getVisibility());
                    Assert.assertEquals(View.GONE, primaryButtonMinorMode.getVisibility());

                    Assert.assertEquals(View.GONE, secondaryButton.getVisibility());
                    Assert.assertEquals(View.GONE, secondaryButtonMinorMode.getVisibility());
                });

        // Capability is received as MINOR_MODE_REQUIRED after an arbitrary amount of time that is
        // less than the deadline {@link MinorModeHelper.CAPABILITY_TIMEOUT_MS}. Buttons
        // will be equally weighted.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninTestRule.resolveMinorModeToRestricted(
                            AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT.getId());
                });

        onViewWaiting(withId(org.chromium.chrome.R.id.button_secondary_minor_mode));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button primaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_primary_minor_mode);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Button secondaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_secondary_minor_mode);

                    // Buttons for non minor mode users should still be hidden
                    Assert.assertEquals(View.GONE, primaryButton.getVisibility());
                    Assert.assertEquals(View.GONE, secondaryButton.getVisibility());

                    Assert.assertEquals(View.VISIBLE, primaryButtonMinorMode.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButtonMinorMode.getVisibility());
                    Assert.assertEquals(
                            primaryButtonMinorMode.getTextColors().getDefaultColor(),
                            secondaryButtonMinorMode.getTextColors().getDefaultColor());
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testButtonsEquallyWeightedWithMinorAccountOnDeadline() throws Exception {
        Activity historySyncActivity = mActivityTestRule.getActivity();
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        // History sync opt-in screen should be displayed.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));

        ViewUtils.waitForVisibleView(withId(R.id.button_secondary_minor_mode));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = historySyncActivity.findViewById(R.id.button_primary);
                    Button primaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_primary_minor_mode);
                    Button secondaryButton =
                            historySyncActivity.findViewById(R.id.button_secondary);
                    Button secondaryButtonMinorMode =
                            historySyncActivity.findViewById(R.id.button_secondary_minor_mode);

                    // Button for non minor mode users should still be hidden
                    Assert.assertEquals(View.GONE, primaryButton.getVisibility());
                    Assert.assertEquals(View.GONE, secondaryButton.getVisibility());

                    Assert.assertEquals(View.VISIBLE, primaryButtonMinorMode.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButtonMinorMode.getVisibility());
                    Assert.assertEquals(
                            primaryButtonMinorMode.getTextColors().getDefaultColor(),
                            secondaryButtonMinorMode.getTextColors().getDefaultColor());
                });
    }

    private void buildHistorySyncCoordinator() {
        buildHistorySyncCoordinator(false, false);
    }

    private void buildHistorySyncCoordinator(
            boolean showEmailInFooter, boolean shouldSignOutOnDecline) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncCoordinator =
                            new HistorySyncCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mHistorySyncDelegateMock,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    SIGNIN_ACCESS_POINT,
                                    showEmailInFooter,
                                    shouldSignOutOnDecline,
                                    null);
                    mActivityTestRule
                            .getActivity()
                            .setContentView(mHistorySyncCoordinator.getView());
                });
        ViewUtils.waitForVisibleView(allOf(withId(R.id.history_sync_title), isDisplayed()));
    }
}
