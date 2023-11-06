// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render tests of SyncPromoController. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncPromoControllerUITest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private static final String TEST_EMAIL = "john.doe@gmail.com";

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private SyncConsentActivityLauncher mSyncConsentActivityLauncher;

    @Before
    public void setUp() {
        // TODO(crbug.com/1297981): Remove dependency on ChromeTabbedActivityTestRule.
        // Starting ChromeTabbedActivityTestRule to initialize the browser, which is needed when
        // SyncPromoController.setUpSyncPromoView() is called.
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }

    /**
     * @param nightModeEnabled A nitght mode flag injected by @ParameterAnnotations.ClassParameter.
     */
    public SyncPromoControllerUITest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @MediumTest
    public void testBookmarkSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    mActivityTestRule.getActivity());
                        });
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testBookmarkSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testBookmarkSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingsSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    mActivityTestRule.getActivity());
                        });
        setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        onView(withText(R.string.sync_promo_title_settings)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_settings)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingsSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        onView(withText(R.string.sync_promo_title_settings)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_settings)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingsSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        onView(withText(R.string.sync_promo_title_settings)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_settings)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    mActivityTestRule.getActivity());
                        });
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        onView(withText(R.string.sync_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_recent_tabs)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testRecentTabsSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        onView(withText(R.string.sync_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_recent_tabs)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        onView(withText(R.string.sync_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_recent_tabs)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSetUpSyncPromoView_onNonAutomotive_secondaryButtonShown() throws Throwable {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);

        onView(withId(R.id.sync_promo_choose_account_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSetUpSyncPromoView_onAutomotive_secondaryButtonHidden() throws Throwable {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);

        onView(withId(R.id.sync_promo_choose_account_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    mActivityTestRule.getActivity());
                        });
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view,
                "ntp_content_suggestions_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_in_and_not_syncing");
    }

    // TODO(crbug.com/1314490): In production we observe the onProfileDataUpdated() event and then
    // update the view, but that's done outside of SyncPromoController, the logic is duplicated
    // for each entry point. In the long term, we should have a single observer internal to the UI
    // component. Then these tests can just wait for the right data to appear with espresso.
    private ProfileDataCache createProfileDataCacheAndWaitForAccountData() throws Throwable {
        CallbackHelper profileDataUpdatedWaiter = new CallbackHelper();
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            ProfileDataCache profileData =
                                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                            mActivityTestRule.getActivity());
                            // Observing the  onProfileDataUpdated() event.
                            profileData.addObserver(
                                    (String accountEmail) -> {
                                        profileDataUpdatedWaiter.notifyCalled();
                                    });
                            return profileData;
                        });
        // Waiting for onProfileDataUpdated() to be called.
        profileDataUpdatedWaiter.waitForFirst();
        return profileDataCache;
    }

    private View setUpSyncPromoView(
            @AccessPoint int accessPoint, ProfileDataCache profileDataCache, int layoutResId) {
        View view =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            View promoView =
                                    LayoutInflater.from(mActivityTestRule.getActivity())
                                            .inflate(layoutResId, null);
                            Activity activity = mActivityTestRule.getActivity();
                            LinearLayout content = new LinearLayout(activity);
                            content.addView(
                                    promoView,
                                    new LayoutParams(
                                            ViewGroup.LayoutParams.MATCH_PARENT,
                                            ViewGroup.LayoutParams.WRAP_CONTENT));
                            activity.setContentView(content);
                            SyncPromoController syncPromoController =
                                    new SyncPromoController(
                                            Profile.getLastUsedRegularProfile(),
                                            accessPoint,
                                            mSyncConsentActivityLauncher);
                            syncPromoController.setUpSyncPromoView(
                                    profileDataCache,
                                    promoView.findViewById(R.id.signin_promo_view_container),
                                    accessPoint == SigninAccessPoint.RECENT_TABS ? null : () -> {});
                            return promoView;
                        });
        return view;
    }
}
