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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
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
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

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

    private @Mock WindowAndroid mWindow;
    private @Mock Profile mProfile;
    private @Mock ActivityResultTracker mActivityResultTracker;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock BottomSheetSigninAndHistorySyncCoordinator mCoordinator;
    private @Mock BottomSheetController mBottomSheetController;
    private @Mock Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private @Mock SnackbarManager mSnackbarManager;
    private @Mock DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock Runnable mOnOpenSettings;

    private PersonalizedSigninPromoView mPromoView;
    private SigninPromoCoordinator mPromoCoordinator;
    private SigninPromoDelegate mDelegate;
    private boolean mIsSetupListActive;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        lenient()
                .when(
                        mLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                any(), any(), any(), any(), any(), any(), any(), any(), any(),
                                anyInt()))
                .thenReturn(mCoordinator);
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
        try (var unused = mSigninTestRule.blockGetAccountsUpdate()) {
            setUpSignInPromo(accessPoint);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        assertFalse(mPromoCoordinator.canShowPromo());
                    });
        }
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/compact"})
    // TODO(crbug.com/468024353): Add coverage for two_buttons promo.
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPrimaryButtonClick_compactPromo(@SigninAccessPoint int accessPoint) {
        testPrimaryButtonClick(accessPoint, R.id.signin_promo_primary_button);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPrimaryButtonClick_seamlessSigninDisabled(@SigninAccessPoint int accessPoint) {
        testPrimaryButtonClick(accessPoint, R.id.sync_promo_signin_button);
    }

    private void testPrimaryButtonClick(
            @SigninAccessPoint int accessPoint, @IdRes int primaryButtonId) {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SyncPromo.Continued.Count."
                                + getAccessPointToHistogramName(accessPoint),
                        1);
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(accessPoint, /* hasAccounts= */ true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        onView(withId(primaryButtonId)).perform(click());

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            verify(mCoordinator).startSigninFlow(configCaptor.capture());
        } else {
            verify(mLauncher)
                    .createBottomSheetSigninIntentOrShowError(
                            eq(mActivityTestRule.getActivity()),
                            eq(mProfile),
                            configCaptor.capture(),
                            eq(accessPoint));
        }

        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);

        @HistorySyncConfig.OptInMode
        int expectedHistoryOptInMode =
                (accessPoint == SigninAccessPoint.RECENT_TABS
                                || accessPoint == SigninAccessPoint.HISTORY_PAGE)
                        ? HistorySyncConfig.OptInMode.REQUIRED
                        : HistorySyncConfig.OptInMode.NONE;
        assertEquals(expectedHistoryOptInMode, config.historyOptInMode);

        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
                && accessPoint != SigninAccessPoint.HISTORY_PAGE) {
            assertEquals(WithAccountSigninMode.SEAMLESS_SIGNIN, config.withAccountSigninMode);
            assertNotNull(config.selectedCoreAccountId);
        } else {
            assertEquals(
                    WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                    config.withAccountSigninMode);
            assertNull(config.selectedCoreAccountId);
        }

        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/compact"})
    // TODO(crbug.com/468024353): Add coverage for two_buttons promo.
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSigninBottomSheetStrings_compactPromo(@SigninAccessPoint int accessPoint) {
        testSigninBottomSheetStrings(accessPoint, R.id.signin_promo_primary_button);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSigninBottomSheetStrings_seamlessSigninDisabled(
            @SigninAccessPoint int accessPoint) {
        testSigninBottomSheetStrings(accessPoint, R.id.sync_promo_signin_button);
    }

    private void testSigninBottomSheetStrings(
            @SigninAccessPoint int accessPoint, @IdRes int primaryButtonId) {
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        onView(withId(primaryButtonId)).perform(click());

        // Extract the config passed to the sign-in flow launcher.
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            verify(mCoordinator).startSigninFlow(configCaptor.capture());
        } else {
            verify(mLauncher)
                    .createBottomSheetSigninIntentOrShowError(
                            eq(mActivityTestRule.getActivity()),
                            eq(mProfile),
                            configCaptor.capture(),
                            eq(accessPoint));
        }

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
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/compact"})
    // TODO(crbug.com/468024353): Add coverage for two_buttons promo.
    public void testBookmarksAccountSettingsPromoPrimaryButtonClick_compactPromo() {
        testBookmarksAccountSettingsPromoPrimaryButtonClick(R.id.signin_promo_primary_button);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testBookmarksAccountSettingsPromoPrimaryButtonClick_seamlessSigninDisabled() {
        testBookmarksAccountSettingsPromoPrimaryButtonClick(R.id.sync_promo_signin_button);
    }

    private void testBookmarksAccountSettingsPromoPrimaryButtonClick(@IdRes int primaryButtonId) {
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

        onView(withId(primaryButtonId)).perform(click());

        verify(mOnOpenSettings).run();
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/twoButtons"})
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSecondaryButtonClick_twoButtonsPromo(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // The history page promo is hidden for non-signed in accounts.
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

        onView(withId(R.id.signin_promo_secondary_button)).perform(click());

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mCoordinator).startSigninFlow(configCaptor.capture());
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(
                accessPoint == SigninAccessPoint.RECENT_TABS
                        ? HistorySyncConfig.OptInMode.REQUIRED
                        : HistorySyncConfig.OptInMode.NONE,
                config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSecondaryButtonClick_seamlessSigninDisabled(
            @SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // The history page promo is hidden for non-signed in accounts.
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

        if (accessPoint == SigninAccessPoint.RECENT_TABS) {
            // Recent tabs doesn't support secondary button for non seamless sign-in.
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
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/twoButtons"})
    public void testHistoryPagePromoSecondaryButtonHidden_twoButtonsPromo() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);
        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);
        onView(withId(R.id.signin_promo_secondary_button))
                .check(ViewAssertions.matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testHistoryPagePromoSecondaryButtonHidden_seamlessSigninDisabled() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);
        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);
        onView(withId(R.id.sync_promo_choose_account_button))
                .check(ViewAssertions.matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/twoButtons"})
    public void testBookmarksAccountSettingsPromoSecondaryButtonHidden_twoButtonsPromo() {
        testBookmarksAccountSettingsPromoSecondaryButtonHidden(R.id.signin_promo_secondary_button);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testBookmarksAccountSettingsPromoSecondaryButtonHidden_seamlessSigninDisabled() {
        testBookmarksAccountSettingsPromoSecondaryButtonHidden(
                R.id.sync_promo_choose_account_button);
    }

    private void testBookmarksAccountSettingsPromoSecondaryButtonHidden(
            @IdRes int secondaryButtonId) {
        var histogramWatcher =
                getPromoImpressionHistogramWatcher(
                        SigninAccessPoint.BOOKMARK_MANAGER, /* hasAccounts= */ true);
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();

        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        ViewUtils.waitForVisibleView(withText(R.string.sync_promo_title_bookmarks));
        onView(withId(secondaryButtonId)).check(ViewAssertions.matches(not(isDisplayed())));
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/compact"})
    // TODO(crbug.com/468024353): Add coverage for two_buttons promo.
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testDismissButtonClick_compactPromo(@SigninAccessPoint int accessPoint) {
        testPermanentDismissal(
                accessPoint, R.id.signin_promo_dismiss_button, /* dueToUndoneSignin= */ false);
    }

    @Test
    @MediumTest
    @EnableFeatures({"EnableSeamlessSignin" + ":seamless-signin-promo-type/compact"})
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testUndoButtonClick_compactPromo(@SigninAccessPoint int accessPoint) {
        testPermanentDismissal(
                accessPoint, R.id.signin_promo_dismiss_button, /* dueToUndoneSignin= */ true);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/448227402): Remove this test once Seamless Sign-in is launched.
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testDismissButtonClick_seamlessSigninDisabled(@SigninAccessPoint int accessPoint) {
        testPermanentDismissal(
                accessPoint, R.id.sync_promo_close_button, /* dueToUndoneSignin= */ false);
    }

    private void testPermanentDismissal(
            @SigninAccessPoint int accessPoint,
            @IdRes int dismissButtonId,
            boolean dueToUndoneSignin) {
        String event =
                dueToUndoneSignin
                        ? SigninPromoMediator.Event.SIGNIN_UNDONE
                        : SigninPromoMediator.Event.DISMISSED;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.Promo.ImpressionsUntil."
                                        + event
                                        + "."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .expectIntRecord(
                                "Signin.SyncPromo."
                                        + event
                                        + ".Count."
                                        + getAccessPointToHistogramName(accessPoint),
                                1)
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(
                        accessPoint,
                        /* hasAccounts= */ accessPoint == SigninAccessPoint.HISTORY_PAGE);

        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);

        if (!mDelegate.canBeDismissedPermanently()) {
            // Eg.: Recent tabs doesn't support dismiss functionality
            onView(withId(dismissButtonId)).check(ViewAssertions.matches(not(isDisplayed())));
            return;
        }

        assertTrue(mDelegate.canBeDismissedPermanently());
        if (dueToUndoneSignin) {
            // Wait for promo to be shown (for recording impressions), then undo the sign-in flow.
            onView(withId(dismissButtonId)).check(ViewAssertions.matches(isDisplayed()));
            ThreadUtils.runOnUiThreadBlocking(() -> mPromoCoordinator.onSigninUndone());
        } else {
            // Dismiss the promo via [x] dismiss button.
            onView(withId(dismissButtonId)).perform(click());
        }

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
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void testHistorySyncOptIn_RecentTabs() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.RECENT_TABS);
        setUpSignInPromo(SigninAccessPoint.RECENT_TABS);

        onView(withId(R.id.signin_promo_primary_button)).perform(click());

        @HistorySyncConfig.OptInMode
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mCoordinator).startSigninFlow(configCaptor.capture());
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        Context context = mActivityTestRule.getActivity();
        assertEquals(
                context.getString(R.string.history_sync_recent_tabs_title),
                config.historySyncConfig.title);
        assertEquals(
                context.getString(R.string.history_sync_recent_tabs_subtitle),
                config.historySyncConfig.subtitle);
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
                "NoAccountThenWithAccount_noAccount_twoButtons_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_withAccount_twoButtons_"
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
                "NoAccountThenWithAccount_noAccount_compact_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mRenderTestRule.render(
                mPromoView,
                "NoAccountThenWithAccount_withAccount_compact_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));
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
    public void testRendering_seamlessSigninPromo_twoButtons_noAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }

        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView,
                "NoAccount_SeamlessSigninPromo_twoButtons_"
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
    public void testRendering_seamlessSigninPromo_compact_noAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }

        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView,
                "NoAccount_SeamlessSigninPromo_compact_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));
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
    public void testRendering_seamlessSigninPromo_twoButtons_withAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SeamlessSigninPromo_twoButtons_"
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
    public void testRendering_seamlessSigninPromo_compact_withAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo hidden for the history page.
            return;
        }
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SeamlessSigninPromo_compact_"
                        + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_seamlessSigninPromo_twoButtons_signedIn_bookmarks(
            boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SeamlessSigninPromo_twoButtons_SignedIn_"
                        + getParamToRenderId(SigninAccessPoint.BOOKMARK_MANAGER, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_seamlessSigninPromo_compact_signedIn_bookmarks(
            boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SeamlessSigninPromo_compact_SignedIn_"
                        + getParamToRenderId(SigninAccessPoint.BOOKMARK_MANAGER, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_seamlessSigninPromo_signedInThenSignedOut_bookmarks(
            boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedInThenSignedOut_SignedIn_"
                        + (nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled"));

        mSigninTestRule.signOut();

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedInThenSignedOut_SignedOut_"
                        + (nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled"));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_seamlessSigninPromo_signedOutThenSignedIn_bookmarks(
            boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedOutThenSignedIn_SignedOut_"
                        + (nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled"));

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedOutThenSignedIn_SignedIn_"
                        + (nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled"));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_loadingState_Compact(boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPromoCoordinator.setLoadingStateForTesting(true);
                });

        mRenderTestRule.render(
                mPromoView,
                "LoadingState_Compact_"
                        + getParamToRenderId(
                                SigninAccessPoint.NTP_FEED_TOP_PROMO, nightModeEnabled));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRendering_loadingState_TwoButtons(boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPromoCoordinator.setLoadingStateForTesting(true);
                });

        mRenderTestRule.render(
                mPromoView,
                "LoadingState_TwoButtons_"
                        + getParamToRenderId(
                                SigninAccessPoint.NTP_FEED_TOP_PROMO, nightModeEnabled));
    }

    @Test
    @MediumTest
    public void testNtpPromoSuppressed_setupListActive() {
        mIsSetupListActive = true;
        setUpSignInPromo(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        ThreadUtils.runOnUiThreadBlocking(() -> assertFalse(mPromoCoordinator.canShowPromo()));
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
                    mDelegate = getSigninPromoDelegate(accessPoint, activity);
                    mPromoCoordinator =
                            new SigninPromoCoordinator(
                                    mWindow,
                                    activity,
                                    mProfile,
                                    mActivityResultTracker,
                                    mLauncher,
                                    SupplierUtils.of(mBottomSheetController),
                                    mModalDialogManagerSupplier,
                                    mSnackbarManager,
                                    mDeviceLockActivityLauncher,
                                    mDelegate);
                    mPromoCoordinator.setView(mPromoView);
                });
    }

    private SigninPromoDelegate getSigninPromoDelegate(
            @SigninAccessPoint int accessPoint, Activity activity) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER ->
                    new BookmarkSigninPromoDelegate(
                            activity, mProfile, mLauncher, mOnPromoStateChange, mOnOpenSettings);
            case SigninAccessPoint.HISTORY_PAGE ->
                    new HistoryPageSigninPromoDelegate(
                            activity,
                            mProfile,
                            mLauncher,
                            mOnPromoStateChange,
                            /* isCreatedInCct= */ false);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO ->
                    new NtpSigninPromoDelegate(
                            activity,
                            mProfile,
                            mLauncher,
                            mOnPromoStateChange,
                            () -> mIsSetupListActive);
            case SigninAccessPoint.RECENT_TABS ->
                    new RecentTabsSigninPromoDelegate(
                            activity, mProfile, mLauncher, mOnPromoStateChange);
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
        };
    }

    private static String getAccessPointToRenderId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> "BookmarkManager";
            case SigninAccessPoint.HISTORY_PAGE -> "HistoryPage";
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> "NtpFeedTopPromo";
            case SigninAccessPoint.RECENT_TABS -> "RecentTabs";
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
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
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
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
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
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
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
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
