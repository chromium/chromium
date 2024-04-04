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

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.ViewUtils;

/** Tests for the standalone history sync consent dialog */
@RunWith(BaseJUnit4ClassRunner.class)
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

    private HistorySyncCoordinator mHistorySyncCoordinator;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        mSigninTestRule.addTestAccountThenSignin();
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        when(mHistorySyncDelegateMock.isLargeScreen()).thenReturn(false);
    }

    @After
    public void tearDown() {
        mSigninTestRule.forceSignOut();
    }

    @Test
    @MediumTest
    public void testHistorySyncLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Started", SIGNIN_ACCESS_POINT);

        buildHistorySyncCoordinator();

        histogramWatcher.assertExpected();
        onView(withId(R.id.sync_consent_title)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_consent_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_account_image)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_accept_button)).check(matches(isDisplayed()));
        onView(withText(R.string.no_thanks)).check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.sync_consent_details_description),
                                withText(R.string.history_sync_footer)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFooterStringWithEmail() {
        String expectedFooter =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.history_sync_signed_in_footer,
                                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN).getEmail());

        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ true, /* shouldSignOutOnDecline= */ false);

        onView(allOf(withId(R.id.sync_consent_details_description), withText(expectedFooter)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testPositiveButton() {
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Completed", SIGNIN_ACCESS_POINT);

        onView(withText(R.string.signin_accept_button)).perform(click());

        histogramWatcher.assertExpected();
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.HISTORY, true);
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.TABS, true);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
    }

    @Test
    @MediumTest
    public void testNegativeButton() {
        buildHistorySyncCoordinator();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Declined", SIGNIN_ACCESS_POINT);

        onView(withText(R.string.no_thanks)).perform(click());

        histogramWatcher.assertExpected();
        verifyNoInteractions(mSyncServiceMock);
        verify(mHistorySyncDelegateMock).dismissHistorySync();
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testNegativeButton_shouldSignOutOnDecline() {
        buildHistorySyncCoordinator(
                /* showEmailInFooter= */ false, /* shouldSignOutOnDecline= */ true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Declined", SIGNIN_ACCESS_POINT);

        onView(withText(R.string.no_thanks)).perform(click());

        histogramWatcher.assertExpected();
        verifyNoInteractions(mSyncServiceMock);
        verify(mHistorySyncDelegateMock, atLeastOnce()).dismissHistorySync();
        CriteriaHelper.pollUiThread(
                () -> mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN) == null);
    }

    @Test
    @MediumTest
    public void testOnSignedOut() {
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
        ViewUtils.waitForVisibleView(allOf(withId(R.id.sync_consent_title), isDisplayed()));
    }
}
