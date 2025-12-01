// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.LayoutRes;
import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.util.ArrayList;
import java.util.List;

@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "This test relies on native initialization")
public class SigninPromoCoordinatorTest {
    private static final List<Integer> sAccessPoints =
            List.of(
                    SigninAccessPoint.BOOKMARK_MANAGER,
                    SigninAccessPoint.HISTORY_PAGE,
                    SigninAccessPoint.NTP_FEED_TOP_PROMO,
                    SigninAccessPoint.RECENT_TABS);

    public static class AccessPointParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> params = new ArrayList<>();
            for (var accessPoint : sAccessPoints) {
                params.add(
                        new ParameterSet()
                                .value(accessPoint)
                                .name(getAccessPointToRenderId(accessPoint)));
            }
            return params;
        }
    }

    public static class RenderTestParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> params = new ArrayList<>();
            var nightModeParams = List.of(true, false);
            for (var accessPoint : sAccessPoints) {
                for (var nightModeParam : nightModeParams) {
                    params.add(
                            new ParameterSet()
                                    .value(accessPoint, nightModeParam)
                                    .name(getParamToRenderId(accessPoint, nightModeParam)));
                }
            }
            return params;
        }
    }

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private @Mock Profile mProfile;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock Runnable mOnOpenSettings;

    private PersonalizedSigninPromoView mPromoView;
    private SigninPromoCoordinator mPromoCoordinator;
    private SigninPromoDelegate mDelegate;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
    }

    @ParameterAnnotations.UseMethodParameterBefore(RenderTestParams.class)
    public void setUpRenderTest(@SigninAccessPoint int accessPoint, boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setUpNightModeTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
    }

    @After
    public void tearDown() {
        if (mPromoCoordinator != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mPromoCoordinator.destroy());
        }
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_DECLINED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS));
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.HISTORY_PAGE));
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.NTP));
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPromoNotShownWhenAccountsNotAvailable(@SigninAccessPoint int accessPoint) {
        try (var unused = mSigninTestRule.blockGetAccountsUpdate(false)) {
            setUpSignInPromo(accessPoint);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        assertFalse(mPromoCoordinator.canShowPromo());
                    });
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPrimaryButtonClick(@SigninAccessPoint int accessPoint) {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SyncPromo.Continued.Count."
                                + getAccessPointToHistogramName(accessPoint),
                        1);
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(
                        accessPoint,
                        /* hasAccounts= */ accessPoint == SigninAccessPoint.HISTORY_PAGE);
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        @HistorySyncConfig.OptInMode
        int historyOptInMode =
                (accessPoint == SigninAccessPoint.RECENT_TABS
                                || accessPoint == SigninAccessPoint.HISTORY_PAGE)
                        ? HistorySyncConfig.OptInMode.REQUIRED
                        : HistorySyncConfig.OptInMode.NONE;
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityTestRule.getActivity()),
                        eq(mProfile),
                        configCaptor.capture(),
                        eq(accessPoint));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(historyOptInMode, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSigninBottomSheetStrings(@SigninAccessPoint int accessPoint) {
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        // Extract the config passed to the sign-in flow launcher.
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityTestRule.getActivity()),
                        eq(mProfile),
                        configCaptor.capture(),
                        eq(accessPoint));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();

        // Verify bottom sheet strings.
        AccountPickerBottomSheetStrings expectedStrings;
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            expectedStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    mActivityTestRule
                                            .getActivity()
                                            .getString(
                                                    R.string
                                                            .signin_account_picker_bottom_sheet_title))
                            .setSubtitleString(
                                    mActivityTestRule
                                            .getActivity()
                                            .getString(
                                                    R.string
                                                            .signin_account_picker_bottom_sheet_benefits_subtitle))
                            .build();
        } else {
            expectedStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    mActivityTestRule
                                            .getActivity()
                                            .getString(
                                                    R.string
                                                            .signin_account_picker_bottom_sheet_title))
                            .build();
        }
        assertEquals(expectedStrings, config.bottomSheetStrings);
    }

    @Test
    @MediumTest
    public void testBookmarksAccountSettingsPromoPrimaryButtonClick() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.Promo.ImpressionsUntil.Continued."
                                        + getAccessPointToHistogramName(
                                                SigninAccessPoint.BOOKMARK_MANAGER),
                                1)
                        .expectIntRecord(
                                "Signin.SyncPromo.Continued.Count."
                                        + getAccessPointToHistogramName(
                                                SigninAccessPoint.BOOKMARK_MANAGER),
                                1)
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(
                        SigninAccessPoint.BOOKMARK_MANAGER, /* hasAccounts= */ true);
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);
        ViewUtils.waitForVisibleView(withText(R.string.sync_promo_title_bookmarks));

        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        verify(mOnOpenSettings).run();
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSecondaryButtonClick(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // The history page promo doesn't have a secondary button.
            return;
        }
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.Promo.ImpressionsUntil.Continued."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .expectIntRecord(
                                "Signin.SyncPromo.Continued.Count."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(accessPoint, /* hasAccounts= */ true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        // TODO(crbug.com/448227402): remove this check once Seamless Sign-in is launched
        if (accessPoint == SigninAccessPoint.RECENT_TABS) {
            onView(withId(R.id.sync_promo_choose_account_button))
                    .check(ViewAssertions.matches(not(isDisplayed())));
            return;
        }
        onView(withId(R.id.sync_promo_choose_account_button)).perform(click());

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityTestRule.getActivity()),
                        eq(mProfile),
                        configCaptor.capture(),
                        eq(accessPoint));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testBookmarksAccountSettingsPromoSecondaryButtonHidden() {
        var histogramWatcher =
                getPromoImpressionHistogramWatcher(
                        SigninAccessPoint.BOOKMARK_MANAGER, /* hasAccounts= */ true);
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();

        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        ViewUtils.waitForVisibleView(withText(R.string.sync_promo_title_bookmarks));
        onView(withId(R.id.sync_promo_choose_account_button))
                .check(ViewAssertions.matches(not(isDisplayed())));
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testDismissButtonClick(@SigninAccessPoint int accessPoint) {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.Promo.ImpressionsUntil.Dismissed."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .expectIntRecord(
                                "Signin.SyncPromo.Dismissed.Count."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(
                        accessPoint,
                        /* hasAccounts= */ accessPoint == SigninAccessPoint.HISTORY_PAGE);

        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        // TODO(crbug.com/448227402): remove this check once Seamless Sign-in is launched
        if (accessPoint == SigninAccessPoint.RECENT_TABS) {
            onView(withId(R.id.sync_promo_close_button))
                    .check(ViewAssertions.matches(not(isDisplayed())));
            return;
        }
        onView(withId(R.id.sync_promo_close_button)).perform(click());

        verify(mOnPromoStateChange).run();
        String preferenceName = getAccessPointDismissPreferenceName(accessPoint);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            ChromeSharedPreferences.getInstance()
                                    .readBoolean(preferenceName, false));
                    assertFalse(mPromoCoordinator.canShowPromo());
                });
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPromoImpressionRecorded(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.RECENT_TABS) {
            // Recent tabs doesn't record impressions.
            return;
        }
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        String preferenceName = getAccessPointImpressionPreferenceName(accessPoint);

        // Impression is recorded asynchronously by ImpressionTracker.
        CriteriaHelper.pollUiThread(
                () -> ChromeSharedPreferences.getInstance().readInt(preferenceName, 0) == 1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mPromoCoordinator.canShowPromo());
                });
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testMaxImpressionReached(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            String preferenceName =
                    ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                            SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS);
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            preferenceName, BookmarkSigninPromoDelegate.MAX_IMPRESSIONS_BOOKMARKS);
        } else if (accessPoint == SigninAccessPoint.NTP_FEED_TOP_PROMO) {
            String preferenceName =
                    ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                            SigninPreferencesManager.SigninPromoAccessPointId.NTP);
            ChromeSharedPreferences.getInstance()
                    .writeInt(preferenceName, NtpSigninPromoDelegate.MAX_IMPRESSIONS_NTP);
        } else if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            String preferenceName =
                    ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                            SigninPreferencesManager.SigninPromoAccessPointId.HISTORY_PAGE);
            ChromeSharedPreferences.getInstance()
                    .writeInt(preferenceName, HistoryPageSigninPromoDelegate.MAX_IMPRESSIONS);
        }
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (accessPoint != SigninAccessPoint.RECENT_TABS) {
                        assertFalse(mPromoCoordinator.canShowPromo());
                    } else {
                        assertTrue(mPromoCoordinator.canShowPromo());
                    }
                });
    }

    @Test
    @MediumTest
    public void testHistoryPageImpressionDelay_firstShown() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);

        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mPromoCoordinator.canShowPromo());
                });
    }

    @Test
    @MediumTest
    public void testHistoryPageImpressionDelay_delayNotReached() {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME,
                        System.currentTimeMillis());
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);

        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mPromoCoordinator.canShowPromo());
                });
    }

    @Test
    @MediumTest
    public void testHistoryPageImpressionDelay_delayReached() {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME,
                        System.currentTimeMillis()
                                - HistoryPageSigninPromoDelegate.MIN_DELAY_BETWEEN_IMPRESSIONS_MS);
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);

        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mPromoCoordinator.canShowPromo());
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(RenderTestParams.class)
    public void testRendering_noAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }

        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView, "NoAccount_" + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(RenderTestParams.class)
    public void testRendering_withAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView, "WithAccount_" + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_withAccount_bookmarksAccountSettingsPromo(boolean nightModeEnabled)
            throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SignedIn_"
                        + getParamToRenderId(SigninAccessPoint.BOOKMARK_MANAGER, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(RenderTestParams.class)
    public void testRendering_noAccountThenWithAccount_twoButtons(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }

        setUpSignInPromo(accessPoint);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_noAccount_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_withAccount_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(RenderTestParams.class)
    public void testRendering_noAccountThenWithAccount_compact(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }

        setUpSignInPromo(accessPoint);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_noAccount_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_withAccount_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    private void setUpSignInPromo(@SigninAccessPoint int accessPoint) {
        @LayoutRes int layoutResId = SigninPromoCoordinator.getLayoutResId(accessPoint);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    Activity activity = mActivityTestRule.getActivity();
                    View promoView = LayoutInflater.from(activity).inflate(layoutResId, null);
                    LinearLayout content = new LinearLayout(activity);
                    content.addView(
                            promoView,
                            new LinearLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT));
                    activity.setContentView(content);

                    mPromoView = promoView.findViewById(R.id.signin_promo_view_container);
                    mDelegate =
                            getSigninPromoDelegate(
                                    accessPoint,
                                    activity,
                                    mProfile,
                                    mLauncher,
                                    mOnPromoStateChange,
                                    mOnOpenSettings);
                    mPromoCoordinator = new SigninPromoCoordinator(activity, mProfile, mDelegate);
                    mPromoCoordinator.setView(mPromoView);
                });
    }

    private static SigninPromoDelegate getSigninPromoDelegate(
            @SigninAccessPoint int accessPoint,
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange,
            Runnable onOpenSettings) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> new BookmarkSigninPromoDelegate(
                    context, profile, launcher, onPromoStateChange, onOpenSettings);
            case SigninAccessPoint.HISTORY_PAGE -> new HistoryPageSigninPromoDelegate(
                    context, profile, launcher, onPromoStateChange, /* isCreatedInCct= */ false);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> new NtpSigninPromoDelegate(
                    context, profile, launcher, onPromoStateChange);
            case SigninAccessPoint.RECENT_TABS -> new RecentTabsSigninPromoDelegate(
                    context, profile, launcher, onPromoStateChange);
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getAccessPointToRenderId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> "BookmarkManager";
            case SigninAccessPoint.HISTORY_PAGE -> "HistoryPage";
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> "NtpFeedTopPromo";
            case SigninAccessPoint.RECENT_TABS -> "RecentTabs";
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static HistogramWatcher getPromoImpressionHistogramWatcher(
            @SigninAccessPoint int accessPoint, boolean hasAccounts) {
        String promoActionSuffix = hasAccounts ? "WithDefault" : "NewAccountNoExistingAccount";
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        "Signin.SyncPromo.Shown.Count."
                                + getAccessPointToHistogramName(accessPoint))
                .expectIntRecord("Signin.SignIn.Offered", accessPoint)
                .expectIntRecord("Signin.SignIn.Offered." + promoActionSuffix, accessPoint)
                .build();
    }

    private static String getAccessPointToHistogramName(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> SigninPreferencesManager
                    .SigninPromoAccessPointId.BOOKMARKS;
            case SigninAccessPoint.HISTORY_PAGE -> SigninPreferencesManager.SigninPromoAccessPointId
                    .HISTORY_PAGE;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> SigninPreferencesManager
                    .SigninPromoAccessPointId.NTP;
            case SigninAccessPoint.RECENT_TABS -> SigninPreferencesManager.SigninPromoAccessPointId
                    .RECENT_TABS;
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getAccessPointDismissPreferenceName(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> ChromePreferenceKeys
                    .SIGNIN_PROMO_BOOKMARKS_DECLINED;
            case SigninAccessPoint.HISTORY_PAGE -> ChromePreferenceKeys
                    .SIGNIN_PROMO_HISTORY_PAGE_DECLINED;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> ChromePreferenceKeys
                    .SIGNIN_PROMO_NTP_PROMO_DISMISSED;
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getAccessPointImpressionPreferenceName(
            @SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT
                    .createKey(SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS);
            case SigninAccessPoint.HISTORY_PAGE -> ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT
                    .createKey(SigninPreferencesManager.SigninPromoAccessPointId.HISTORY_PAGE);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT
                    .createKey(SigninPreferencesManager.SigninPromoAccessPointId.NTP);
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getParamToRenderId(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) {
        String head = getAccessPointToRenderId(accessPoint);
        String tail = nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled";
        return head + "_" + tail;
    }

    private void disableBookmarksAndReadingListDataTypes() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.BOOKMARKS, false);
                    syncService.setSelectedType(UserSelectableType.READING_LIST, false);
                });
        assertFalse(SyncTestUtil.isBookmarksAndReadingListEnabled());
    }

    private void signinAndOptOutHistorySyncIfNeeded(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                        syncService.setSelectedType(UserSelectableType.HISTORY, false);
                        syncService.setSelectedType(UserSelectableType.TABS, false);
                    });
            assertFalse(SyncTestUtil.isHistorySyncEnabled());
        }
    }
}
