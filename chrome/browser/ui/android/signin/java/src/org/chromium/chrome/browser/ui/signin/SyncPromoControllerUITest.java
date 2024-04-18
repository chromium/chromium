// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.HistoryOptInMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render tests of SyncPromoController. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncPromoControllerUITest {
    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private static final String TEST_EMAIL = "john.doe@gmail.com";
    private static final AccountPickerBottomSheetStrings BOTTOM_SHEET_STRINGS =
            new AccountPickerBottomSheetStrings.Builder(R.string.sign_in_to_chrome).build();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    @Mock private SigninAndHistoryOptInActivityLauncher mSigninAndHistoryOptInActivityLauncher;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
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
    @DisableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
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
        mSigninTestRule.addTestAccountThenSignin();
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    // TODO(crbug.com/329216953): Move these tests into SyncPromoControllerTest after it's converted
    // to device unit tests.
    @Test
    @MediumTest
    public void testExistsNonGmailAccountReturnsTrue() {
        SigninManager signinManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () ->
                                IdentityServicesProvider.get()
                                        .getSigninManager(
                                                ProfileManager.getLastUsedRegularProfile()));
        List<CoreAccountInfo> accounts =
                List.of(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                "test1@" + SyncPromoController.GMAIL_DOMAIN, "unused"),
                        CoreAccountInfo.createFromEmailAndGaiaId("test2@nongmail.com", "unused"));

        Assert.assertTrue(SyncPromoController.existsNonGmailAccount(signinManager, accounts));
    }

    // TODO(crbug.com/329216953): Move these tests into SyncPromoControllerTest after it's converted
    // to device unit tests.
    @Test
    @MediumTest
    public void testExistsNonGmailAccountReturnsFalse() {
        SigninManager signinManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () ->
                                IdentityServicesProvider.get()
                                        .getSigninManager(
                                                ProfileManager.getLastUsedRegularProfile()));
        List<CoreAccountInfo> accounts =
                List.of(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                "test1@" + SyncPromoController.GMAIL_DOMAIN, "unused"),
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                "test2@" + SyncPromoController.GMAIL_DOMAIN, "unused"));

        Assert.assertFalse(SyncPromoController.existsNonGmailAccount(signinManager, accounts));
    }

    @Test
    @MediumTest
    @EnableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoContinueButtonLaunchesSigninFlow() throws Throwable {
        mSigninTestRule.addAccount("test@" + SyncPromoController.GMAIL_DOMAIN);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        verify(mSigninAndHistoryOptInActivityLauncher)
                .launchActivityIfAllowed(
                        any(Context.class),
                        any(Profile.class),
                        eq(BOTTOM_SHEET_STRINGS),
                        eq(NoAccountSigninMode.ADD_ACCOUNT),
                        eq(WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    @Test
    @MediumTest
    @DisableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoContinueButtonLaunchesSyncFlow() throws Throwable {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(
                        any(Context.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER),
                        any(String.class));
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @EnableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoContinueButtonLaunchesSyncFlowIfSyncDataLeft()
            throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setString(Pref.GOOGLE_SERVICES_LAST_SYNCING_GAIA_ID, "gaia_id");
                });

        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(
                        any(Context.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER),
                        any(String.class));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .clearPref(Pref.GOOGLE_SERVICES_LAST_SYNCING_GAIA_ID);
                });
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @EnableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoChooseAccountButtonLaunchesSigninFlow() throws Throwable {
        mSigninTestRule.addAccount("test@" + SyncPromoController.GMAIL_DOMAIN);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));

        onView(withId(R.id.sync_promo_choose_account_button)).perform(click());

        verify(mSigninAndHistoryOptInActivityLauncher)
                .launchActivityIfAllowed(
                        any(Context.class),
                        any(Profile.class),
                        eq(BOTTOM_SHEET_STRINGS),
                        eq(NoAccountSigninMode.ADD_ACCOUNT),
                        eq(WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET),
                        eq(HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testBookmarkSyncPromoChooseAccountButtonLaunchesSyncFlow() throws Throwable {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.BOOKMARK_MANAGER,
                profileDataCache,
                R.layout.sync_promo_view_bookmarks);
        onView(withText(R.string.sync_promo_title_bookmarks)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_bookmarks)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));

        onView(withId(R.id.sync_promo_choose_account_button)).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForPromoChooseAccountFlow(
                        any(Context.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER),
                        any(String.class));
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
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
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
        mSigninTestRule.addTestAccountThenSignin();
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.SETTINGS, profileDataCache, R.layout.sync_promo_view_settings);
        onView(withText(R.string.sync_promo_title_settings)).check(matches(isDisplayed()));
        onView(withText(R.string.sync_promo_description_settings)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
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
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRecentTabsSyncPromoViewSignedOutAndNoAccountAvailableLaunchesSigninFlow()
            throws Throwable {
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
        onView(withText(R.string.signin_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_promo_description_recent_tabs))
                .check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));

        onView(withText(R.string.signin_promo_turn_on)).perform(click());

        verify(mSigninAndHistoryOptInActivityLauncher)
                .launchActivityForHistorySyncDedicatedFlow(
                        any(Context.class),
                        any(Profile.class),
                        eq(BOTTOM_SHEET_STRINGS),
                        eq(NoAccountSigninMode.ADD_ACCOUNT),
                        eq(WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAccessPoint.RECENT_TABS));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRecentTabsSyncPromoViewSignedOutAndAccountAvailable() throws Throwable {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
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
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRecentTabsSyncPromoViewSignedOutAndAccountAvailableLaunchesSigninFlow()
            throws Throwable {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        onView(withText(R.string.signin_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_promo_description_recent_tabs))
                .check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));

        onView(withText(R.string.signin_promo_turn_on)).perform(click());

        verify(mSigninAndHistoryOptInActivityLauncher)
                .launchActivityForHistorySyncDedicatedFlow(
                        any(Context.class),
                        any(Profile.class),
                        eq(BOTTOM_SHEET_STRINGS),
                        eq(NoAccountSigninMode.ADD_ACCOUNT),
                        eq(WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAccessPoint.RECENT_TABS));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncing() throws Throwable {
        mSigninTestRule.addTestAccountThenSignin();
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
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRecentTabsSyncPromoViewSignedInAndNotSyncingLaunchesSigninFlow()
            throws Throwable {
        mSigninTestRule.addTestAccountThenSignin();
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        setUpSyncPromoView(
                SigninAccessPoint.RECENT_TABS,
                profileDataCache,
                R.layout.sync_promo_view_recent_tabs);
        onView(withText(R.string.signin_promo_title_recent_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_promo_description_recent_tabs))
                .check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).check(matches(not(isDisplayed())));

        onView(withText(R.string.signin_promo_turn_on)).perform(click());

        verify(mSigninAndHistoryOptInActivityLauncher)
                .launchActivityForHistorySyncDedicatedFlow(
                        any(Context.class),
                        any(Profile.class),
                        eq(BOTTOM_SHEET_STRINGS),
                        eq(NoAccountSigninMode.ADD_ACCOUNT),
                        eq(WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAccessPoint.RECENT_TABS));
    }

    @Test
    @MediumTest
    public void testSetUpSyncPromoView_onNonAutomotive_secondaryButtonShown() throws Throwable {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
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
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
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
    @UseMethodParameter(NightModeParams.class)
    public void testNTPSyncPromoViewSignedOutAndNoAccountAvailable(boolean nightModeEnabled)
            throws Throwable {
        setUpNightMode(nightModeEnabled);
        ProfileDataCache profileDataCache =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    mActivityTestRule.getActivity());
                        });
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view,
                "ntp_content_suggestions_sync_promo_view_signed_out_and_no_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testNTPSyncPromoViewSignedOutAndAccountAvailable(boolean nightModeEnabled)
            throws Throwable {
        setUpNightMode(nightModeEnabled);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_out_and_account_available");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testNTPSyncPromoViewSignedInAndNotSyncing(boolean nightModeEnabled)
            throws Throwable {
        setUpNightMode(nightModeEnabled);
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccountAndWaitForSeeding(TEST_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        ProfileDataCache profileDataCache = createProfileDataCacheAndWaitForAccountData();
        View view =
                setUpSyncPromoView(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        profileDataCache,
                        R.layout.sync_promo_view_content_suggestions);
        mRenderTestRule.render(
                view, "ntp_content_suggestions_sync_promo_view_signed_in_and_not_syncing");
    }

    // TODO(crbug.com/40832897): In production we observe the onProfileDataUpdated() event and then
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
                                            ProfileManager.getLastUsedRegularProfile(),
                                            BOTTOM_SHEET_STRINGS,
                                            accessPoint,
                                            mSyncConsentActivityLauncher,
                                            mSigninAndHistoryOptInActivityLauncher);
                            syncPromoController.setUpSyncPromoView(
                                    profileDataCache,
                                    promoView.findViewById(R.id.signin_promo_view_container),
                                    accessPoint == SigninAccessPoint.RECENT_TABS ? null : () -> {});
                            return promoView;
                        });
        return view;
    }

    private void setUpNightMode(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }
}
