// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.view.LayoutInflater;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
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
    /** Stub implementation of the {@link HistorySyncDelegate} for testing */
    public class HistorySyncTestDelegate implements HistorySyncCoordinator.HistorySyncDelegate {
        private boolean mIsDialogClosed;

        @Override
        public void dismiss() {
            mIsDialogClosed = true;
        }

        public boolean isDialogClosed() {
            return mIsDialogClosed;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private SyncService mSyncServiceMock;

    private HistorySyncCoordinator mHistorySyncCoordinator;
    private HistorySyncTestDelegate mDelegate;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        mSigninTestRule.addTestAccountThenSignin();
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    @Test
    @MediumTest
    public void testHistorySyncLayout() {
        buildHistorySyncCoordinator();

        onView(withId(R.id.sync_consent_title)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_consent_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.account_image)).check(matches(isDisplayed()));
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(isDisplayed()));
        onView(withId(R.id.negative_button)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_consent_details_description)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testPositiveButton() {
        buildHistorySyncCoordinator();

        onView(withId(R.id.positive_button)).perform(click());

        verify(mSyncServiceMock).setSelectedType(UserSelectableType.HISTORY, true);
        verify(mSyncServiceMock).setSelectedType(UserSelectableType.TABS, true);
        Assert.assertTrue(mDelegate.isDialogClosed());
    }

    @Test
    @MediumTest
    public void testNegativeButton() {
        buildHistorySyncCoordinator();

        onView(withId(R.id.negative_button)).perform(click());
        verifyNoInteractions(mSyncServiceMock);
        Assert.assertTrue(mDelegate.isDialogClosed());
    }

    private void buildHistorySyncCoordinator() {
        mDelegate = new HistorySyncTestDelegate();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncCoordinator =
                            new HistorySyncCoordinator(
                                    LayoutInflater.from(mActivityTestRule.getActivity()),
                                    mActivityTestRule.getActivity().findViewById(R.id.container),
                                    mDelegate,
                                    ProfileManager.getLastUsedRegularProfile());
                    mActivityTestRule
                            .getActivity()
                            .setContentView(mHistorySyncCoordinator.getView());
                });
        ViewUtils.waitForVisibleView(allOf(withId(R.id.sync_consent_title), isDisplayed()));
    }
}
