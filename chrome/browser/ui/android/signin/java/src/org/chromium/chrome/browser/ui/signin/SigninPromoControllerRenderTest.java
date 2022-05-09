// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.support.test.runner.lifecycle.Stage;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render tests of SigninPromoController. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninPromoControllerRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final String TEST_EMAIL = "john.doe@gmail.com";

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private SyncConsentActivityLauncher mSyncConsentActivityLauncher;

    @Before
    public void setUp() {
        // TODO(crbug.com/1297981): Remove dependency on ChromeTabbedActivityTestRule.
        // Starting ChromeTabbedActivityTestRule to initialize the browser, which is needed when
        // SigninPromoController.setUpSyncPromoView() is called.
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }

    /**
     * @param nightModeEnabled A nitght mode flag injected by @ParameterAnnotations.ClassParameter.
     */
    public SigninPromoControllerRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    // TODO(crbug.com/1314490): In production we observe the onProfileDataUpdated() event and then
    // update the view, but that's done outside of SigninPromoController, the logic is duplicated
    // for each entry point. In the long term, we should have a single observer internal to the UI
    // component. Then these tests can just wait for the right data to appear with espresso.
    private ProfileDataCache createProfileDataCacheAndWaitForAccountData() throws Throwable {
        CallbackHelper profileDataUpdatedWaiter = new CallbackHelper();
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileData = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
            // Observing the  onProfileDataUpdated() event.
            profileData.addObserver(
                    (String accountEmail) -> { profileDataUpdatedWaiter.notifyCalled(); });
            return profileData;
        });
        // Waiting for onProfileDataUpdated() to be called.
        profileDataUpdatedWaiter.waitForFirst();
        return profileDataCache;
    }

    private void setContentViewAndSetUpSyncPromoView(
            View view, @AccessPoint int accessPoint, ProfileDataCache profileDataCache) {
        ThreadUtils.assertOnUiThread();
        Activity activity = mActivityTestRule.getActivity();
        LinearLayout content = new LinearLayout(activity);
        content.addView(view,
                new LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        activity.setContentView(content);
        SigninPromoController signinPromoController =
                new SigninPromoController(accessPoint, mSyncConsentActivityLauncher);
        signinPromoController.setUpSyncPromoView(profileDataCache,
                view.findViewById(R.id.signin_promo_view_container),
                accessPoint == SigninAccessPoint.RECENT_TABS ? null : () -> {});
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithFeaturesDisabled() throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "bookmark_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "bookmark_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "bookmark_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithFeaturesDisabled() throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "bookmark_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "bookmark_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "bookmark_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testBookmarkSyncPromoViewSignedInAndNotSyncingWithFeaturesDisabled() throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "bookmark_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void testBookmarkSyncPromoViewSignedInAndNotSyncingWithSingleButtonFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "bookmark_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testBookmarkSyncPromoViewSignedInAndNotSyncingWithTitleFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_bookmarks, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "bookmark_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithFeaturesDisabled() throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "settings_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "settings_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "settings_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithFeaturesDisabled() throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "settings_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "settings_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testSettingsSyncPromoViewSignedOutAndAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "settings_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testSettingsSyncPromoViewSignedInAndNotSyncingWithFeaturesDisabled() throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "settings_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void testSettingsSyncPromoViewSignedInAndNotSyncingWithSingleButtonFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "settings_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testSettingsSyncPromoViewSignedInAndNotSyncingWithTitleFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_settings, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.SETTINGS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "settings_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithFeaturesDisabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithFeaturesDisabled() throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testRecentTabsSyncPromoViewSignedInAndNotSyncingWithFeaturesDisabled() throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "recent_tabs_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncingWithSingleButtonFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncingWithTitleFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(R.layout.personalized_signin_promo_view_recent_tabs, null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.RECENT_TABS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view, "recent_tabs_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndNoAccountAvailableWithFeaturesDisabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndNoAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ProfileDataCache profileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mActivityTestRule.getActivity());
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndAccountAvailableWithFeaturesDisabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndAccountAvailableWithSingleButtonFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedOutAndAccountAvailableWithTitleFeatureEnabled()
            throws Throwable {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedInAndNotSyncingWithFeaturesDisabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    public void
    testNTPContentSuggestionsSyncPromoViewSignedInAndNotSyncingWithSingleButtonFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE})
    @DisableFeatures({ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void testNTPContentSuggestionsSyncPromoViewSignedInAndNotSyncingWithTitleFeatureEnabled()
            throws Throwable {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity())
                            .inflate(
                                    R.layout.personalized_signin_promo_view_modern_content_suggestions,
                                    null);
            setContentViewAndSetUpSyncPromoView(
                    promoView, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache);
            return promoView;
        });
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_title_signed_in_and_not_syncing");
    }
}