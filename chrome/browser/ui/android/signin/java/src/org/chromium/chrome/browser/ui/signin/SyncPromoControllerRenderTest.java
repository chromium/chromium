// Copyright 2022 The Chromium Authors
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
@DisableFeatures({
        ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
        ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
        ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
        ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
})
public class SyncPromoControllerRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final String TEST_EMAIL = "john.doe@gmail.com";

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(7)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

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
        // SyncPromoController.setUpSyncPromoView() is called.
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }

    /**
     * @param nightModeEnabled A nitght mode flag injected by @ParameterAnnotations.ClassParameter.
     */
    public SyncPromoControllerRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testBookmarkSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithIllustration() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_illustration_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButton() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndNoAccountAvailableWithTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testBookmarkSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithIllustration() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_illustration_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithSingleButton() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(
                view, "bookmark_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testBookmarkSyncPromoViewSignedOutAndAccountAvailableWithTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testBookmarkSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testBookmarkSyncPromoViewSignedInAndNotSyncingWithIllustration() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_illustration_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testBookmarkSyncPromoViewSignedInAndNotSyncingWithSingleButton() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testBookmarkSyncPromoViewSignedInAndNotSyncingWithTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.BOOKMARK_MANAGER, profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        mRenderTestRule.render(view, "bookmark_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSettingsSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithAlternativeTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_alternative_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithIllustration() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_illustration_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButton() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndNoAccountAvailableWithTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSettingsSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithAlternativeTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_alternative_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithIllustration() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_illustration_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithSingleButton() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedOutAndAccountAvailableWithTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSettingsSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedInAndNotSyncingWithAlternativeTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(
                view, "settings_sync_promo_alternative_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testSettingsSyncPromoViewSignedInAndNotSyncingWithIllustration() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_illustration_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testSettingsSyncPromoViewSignedInAndNotSyncingWithSingleButton() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testSettingsSyncPromoViewSignedInAndNotSyncingWithTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        mRenderTestRule.render(view, "settings_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithAlternativeTitle()
            throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(view,
                "recent_tabs_sync_promo_alternative_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithIllustration() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_illustration_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButton() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableWithTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testRecentTabsSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithAlternativeTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_alternative_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithIllustration() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_illustration_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithSingleButton() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedOutAndAccountAvailableWithTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(view, "recent_tabs_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedInAndNotSyncingWithAlternativeTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_alternative_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testRecentTabsSyncPromoViewSignedInAndNotSyncingWithIllustration() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_illustration_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testRecentTabsSyncPromoViewSignedInAndNotSyncingWithSingleButton() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(
                view, "recent_tabs_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testRecentTabsSyncPromoViewSignedInAndNotSyncingWithTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.RECENT_TABS, profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        mRenderTestRule.render(view, "recent_tabs_sync_promo_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedOutAndNoAccountAvailable() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedOutAndNoAccountAvailableWithAlternativeTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_alternative_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testNTPSyncPromoViewSignedOutAndNoAccountAvailableWithIllustration() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_illustration_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testNTPSyncPromoViewSignedOutAndNoAccountAvailableWithSingleButton() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_single_button_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedOutAndNoAccountAvailableWithTitle() throws Throwable {
        ProfileDataCache profileDataCache = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                    mActivityTestRule.getActivity());
        });
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_title_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedOutAndAccountAvailableWithAlternativeTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_alternative_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testNTPSyncPromoViewSignedOutAndAccountAvailableWithIllustration() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_illustration_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testNTPSyncPromoViewSignedOutAndAccountAvailableWithSingleButton() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_single_button_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedOutAndAccountAvailableWithTitle() throws Throwable {
        mSigninTestRule.addAccount(TEST_EMAIL);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_title_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNTPSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedInAndNotSyncingWithAlternativeTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(view,
                "ntp_content_suggestions_sync_promo_alternative_title_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
    })
    public void
    testNTPSyncPromoViewSignedInAndNotSyncingWithIllustration() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_illustration_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
    })
    public void
    testNTPSyncPromoViewSignedInAndNotSyncingWithSingleButton() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_single_button_signed_in_and_not_syncing");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testNTPSyncPromoViewSignedInAndNotSyncingWithTitle() throws Throwable {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view = setUpSyncPromoView(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, profileDataCache,
                R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_title_signed_in_and_not_syncing");
    }

    // TODO(crbug.com/1314490): In production we observe the onProfileDataUpdated() event and then
    // update the view, but that's done outside of SyncPromoController, the logic is duplicated
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

    private View setUpSyncPromoView(
            @AccessPoint int accessPoint, ProfileDataCache profileDataCache, int layoutResId) {
        View view = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View promoView =
                    LayoutInflater.from(mActivityTestRule.getActivity()).inflate(layoutResId, null);
            Activity activity = mActivityTestRule.getActivity();
            LinearLayout content = new LinearLayout(activity);
            content.addView(promoView,
                    new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
            activity.setContentView(content);
            SyncPromoController syncPromoController =
                    new SyncPromoController(accessPoint, mSyncConsentActivityLauncher);
            syncPromoController.setUpSyncPromoView(profileDataCache,
                    promoView.findViewById(R.id.signin_promo_view_container),
                    accessPoint == SigninAccessPoint.RECENT_TABS ? null : () -> {});
            return promoView;
        });
        return view;
    }
}