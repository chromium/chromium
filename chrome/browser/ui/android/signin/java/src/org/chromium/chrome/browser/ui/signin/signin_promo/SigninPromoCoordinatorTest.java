// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.LargeTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.function.ThrowingRunnable;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.TestThreadUtils;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.AccountPreviewPreference;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninFunnelEvent;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.protocol.SyncEnums;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link SigninPromoCoordinator}.
 *
 * <p>All scenarios run sequentially inside a single test method against one shared {@link
 * BlankUiTestActivity} and browser process, so they are deliberately not independent: {@link
 * #runScenario} creates fresh mocks before each scenario and {@link #resetState()} tears down the
 * promo, prefs, night mode and sign-in state after it.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SigninPromoCoordinatorTest {
    private static final List<Integer> sAccessPoints =
            List.of(
                    SigninAccessPoint.BOOKMARK_MANAGER,
                    SigninAccessPoint.HISTORY_PAGE,
                    SigninAccessPoint.NTP_FEED_TOP_PROMO,
                    SigninAccessPoint.RECENT_TABS);
    private static final List<Boolean> sNightModeParams = List.of(false, true);

    private interface AccessPointScenario {
        void run(@SigninAccessPoint int accessPoint) throws Throwable;
    }

    private interface NightModeScenario {
        void run(boolean nightModeEnabled) throws Throwable;
    }

    private interface AccessPointNightModeScenario {
        void run(@SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Throwable;
    }

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    @Rule(order = 1)
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    // Mocks are recreated by createMocks() before each scenario so that stubs and recorded
    // invocations never carry over from one scenario to the next.
    private @Mock WindowAndroid mWindow;
    private @Mock Profile mProfile;
    private @Mock ActivityResultTracker mActivityResultTracker;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock BottomSheetSigninAndHistorySyncCoordinator mCoordinator;
    private @Mock BottomSheetController mBottomSheetController;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock SnackbarManager mSnackbarManager;
    private @Mock DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private @Mock ExternalAuthUtils mExternalAuthUtilsMock;
    private @Mock AccountPreviewDataService mAccountPreviewDataServiceMock;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock Runnable mOnOpenSettings;

    private PersonalizedSigninPromoView mPromoView;
    private SigninPromoCoordinator mPromoCoordinator;
    private SigninPromoDelegate mDelegate;
    private boolean mIsNativeInitialized;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().hasWindowFocus(),
                "Activity did not gain window focus.");
    }

    @After
    public void tearDown() {
        resetState();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninPromo() throws Throwable {
        // This scenario has to run first: RefreshTokensLoadingState.IN_PROGRESS only occurs before
        // the initial native startup, which the first setUpSignInPromo() call triggers.
        runScenario(this::checkPromoShownAfterAccountsBecomeAvailable);

        forEachAccessPoint(this::checkPrimaryButtonClick);
        forEachAccessPoint(this::checkLoadingStateIsShownDuringSignIn);
        forEachAccessPoint(this::checkSigninBottomSheetStrings);
        runScenario(this::checkBookmarksAccountSettingsPromoPrimaryButtonClick);
        runScenario(this::checkLoadingStateNotShownForBookmarksSettingsPromo);
        forEachAccessPoint(this::checkAccountPickerClick);
        runScenario(this::checkHistoryPagePromoAccountPickerHidden);
        runScenario(this::checkBookmarksAccountSettingsPromoAccountPickerHidden);
        forEachAccessPoint(ap -> checkPermanentDismissal(ap, /* dueToUndoneSignin= */ false));
        forEachAccessPoint(ap -> checkPermanentDismissal(ap, /* dueToUndoneSignin= */ true));
        forEachAccessPoint(this::checkPromoImpressionRecorded);
        forEachAccessPoint(this::checkMaxImpressionReached);
        runScenario(this::checkHistoryPageImpressionDelay_firstShown);
        runScenario(this::checkHistoryPageImpressionDelay_delayNotReached);
        runScenario(this::checkHistoryPageImpressionDelay_delayReached);
        runScenario(this::checkHistorySyncOptIn_RecentTabs);

        forEachNightMode(this::checkRendering_withAccount_bookmarksAccountSettingsPromo);
        forEachAccessPointAndNightMode(this::checkRendering_noAccountThenWithAccount_compact);
        forEachAccessPointAndNightMode(this::checkRendering_seamlessSigninPromo_compact_noAccount);
        forEachAccessPointAndNightMode(
                this::checkRendering_seamlessSigninPromo_compact_withAccount);
        forEachNightMode(this::checkRendering_seamlessSigninPromo_compact_signedIn_bookmarks);
        forEachNightMode(this::checkRendering_seamlessSigninPromo_signedInThenSignedOut_bookmarks);
        forEachNightMode(this::checkRendering_seamlessSigninPromo_signedOutThenSignedIn_bookmarks);
        forEachNightMode(this::checkRendering_loadingState_Compact);

        // These scenarios override ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT. FeatureOverrides are
        // only reset at the end of the test method, so keep them last.
        forEachAccessPoint(this::checkVisibleAccountWithPreferredAccount_preferredAccountEnabled);
        forEachAccessPoint(this::checkVisibleAccountWithPreferredAccount_preferredAccountDisabled);
    }

    private void checkPromoShownAfterAccountsBecomeAvailable() {
        assertFalse(
                "This scenario must run before native is initialized.",
                ProfileManager.isInitialized());
        record BlockedPromo(
                @SigninAccessPoint int accessPoint,
                View content,
                SigninPromoCoordinator coordinator,
                Runnable onPromoStateChange) {}
        List<BlockedPromo> promos = new ArrayList<>();
        try (var _ = mSigninTestRule.blockGetAccountsUpdate()) {
            for (@SigninAccessPoint int accessPoint : sAccessPoints) {
                var histogramWatcher =
                        HistogramWatcher.newBuilder()
                                .expectNoRecords(
                                        "Signin.SigninFunnel."
                                                + getAccessPointToRenderId(accessPoint))
                                .build();
                Runnable onPromoStateChange = mock(Runnable.class);
                mOnPromoStateChange = onPromoStateChange;
                setUpSignInPromo(accessPoint);
                SigninPromoCoordinator coordinator = mPromoCoordinator;
                mPromoCoordinator = null;
                View content =
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    assertFalse(
                                            "Promo should not show for accessPoint=" + accessPoint,
                                            coordinator.canShowPromo());
                                    ViewGroup root =
                                            mActivityTestRule
                                                    .getActivity()
                                                    .findViewById(android.R.id.content);
                                    return root.getChildAt(0);
                                });
                histogramWatcher.assertExpected();
                promos.add(new BlockedPromo(accessPoint, content, coordinator, onPromoStateChange));
            }
        }
        try {
            for (BlockedPromo promo : promos) {
                if (promo.accessPoint == SigninAccessPoint.HISTORY_PAGE) {
                    // Promo is only shown for signed-in users on the history page.
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> assertFalse(promo.coordinator.canShowPromo()));
                    continue;
                }
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().setContentView(promo.content));
                CriteriaHelper.pollUiThread(() -> promo.coordinator.canShowPromo());
                waitForView(
                        withId(R.id.signin_promo_primary_button),
                        withText(R.string.sync_promo_continue));
                verify(promo.onPromoStateChange).run();
                onView(withId(R.id.account_picker_selected_account))
                        .check(matches(not(isDisplayed())));
            }
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        for (BlockedPromo promo : promos) {
                            promo.coordinator.destroy();
                        }
                    });
        }
    }

    private void checkVisibleAccountWithPreferredAccount_preferredAccountEnabled(
            @SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo is only shown for signed-in users on the history page.
            return;
        }

        FeatureOverrides.enable(SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);
        AccountPreviewPreference preference =
                new AccountPreviewPreference(
                        TestAccounts.ACCOUNT2.getGaiaId(),
                        new int[0],
                        SyncEnums.DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED);
        when(mAccountPreviewDataServiceMock.getPreferredAccountForSignin()).thenReturn(preference);
        setUpSignInPromo(accessPoint);

        // Since the flag is enabled and Account2 is preferred, the promo should show Account2.
        waitForView(
                withText(
                        mActivityTestRule
                                .getActivity()
                                .getString(
                                        R.string.sync_promo_continue_as,
                                        TestAccounts.ACCOUNT2.getGivenName())));
    }

    private void checkVisibleAccountWithPreferredAccount_preferredAccountDisabled(
            @SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo is only shown for signed-in users on the history page.
            return;
        }

        FeatureOverrides.disable(SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);
        setUpSignInPromo(accessPoint);

        // When the feature is disabled, default to Account1.
        waitForView(
                withText(
                        mActivityTestRule
                                .getActivity()
                                .getString(
                                        R.string.sync_promo_continue_as,
                                        TestAccounts.ACCOUNT1.getGivenName())));
        verify(mAccountPreviewDataServiceMock, never()).getPreferredAccountForSignin();
    }

    private void checkPrimaryButtonClick(@SigninAccessPoint int accessPoint) {
        var histogramWatcherBuilder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncPromo.Continued.Count."
                                        + getAccessPointToHistogramName(accessPoint),
                                1);
        // Sign-in funnel events are only recorded for signed-out sign-in promos, whereas the
        // history page only shows a history sync promo to already signed-in users.
        if (accessPoint != SigninAccessPoint.HISTORY_PAGE) {
            histogramWatcherBuilder.expectIntRecords(
                    "Signin.SigninFunnel." + getAccessPointToRenderId(accessPoint),
                    SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT,
                    SigninFunnelEvent.SIGNIN_BUTTON_CLICKED);
        } else {
            histogramWatcherBuilder.expectNoRecords("Signin.SigninFunnel.HistoryPage");
        }
        var histogramWatcher = histogramWatcherBuilder.build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(accessPoint, /* hasAccounts= */ true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);
        waitForView(withId(R.id.signin_promo_primary_button)).click();

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mCoordinator).startSigninFlow(configCaptor.capture());

        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);

        @HistorySyncConfig.OptInMode
        int expectedHistoryOptInMode =
                (accessPoint == SigninAccessPoint.RECENT_TABS
                                || accessPoint == SigninAccessPoint.HISTORY_PAGE)
                        ? HistorySyncConfig.OptInMode.REQUIRED
                        : HistorySyncConfig.OptInMode.NONE;
        assertEquals(expectedHistoryOptInMode, config.historyOptInMode);

        if (accessPoint != SigninAccessPoint.HISTORY_PAGE) {
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

    private void checkLoadingStateIsShownDuringSignIn(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            // Promo is only shown for signed-in users on the history page and does not support
            // seamless sign-in.
            return;
        }

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);
        waitForView(withId(R.id.signin_promo_primary_button));

        String expectedPrimaryButtonText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_promo_continue_as,
                                TestAccounts.ACCOUNT1.getGivenName());

        // Before click: primary button and secondary CTA are enabled.
        onView(withId(R.id.signin_promo_primary_button))
                .check(matches(allOf(isEnabled(), withText(expectedPrimaryButtonText))));
        onView(withId(R.id.account_picker_selected_account))
                .check(matches(allOf(isDisplayed(), isEnabled())));
        if (mDelegate.canBeDismissedPermanently()) {
            onView(withId(R.id.signin_promo_dismiss_button))
                    .check(matches(allOf(isDisplayed(), isEnabled())));
        } else {
            onView(withId(R.id.signin_promo_dismiss_button))
                    .check(
                            matches(
                                    allOf(
                                            not(isEnabled()),
                                            withEffectiveVisibility(Visibility.INVISIBLE))));
        }

        onView(withId(R.id.signin_promo_primary_button)).perform(click());

        // Verify that sign-in flow started.
        verify(mCoordinator).startSigninFlow(any());

        // 1. Primary button text changes to "Signing in...".
        onView(withId(R.id.signin_promo_primary_button))
                .check(matches(withText(R.string.signin_account_picker_bottom_sheet_signin_title)));

        // 2. Primary CTA, secondary CTA, and dismiss button are disabled.
        onView(withId(R.id.signin_promo_primary_button)).check(matches(not(isEnabled())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isEnabled())));
        // 3. Dismiss button is hidden and disabled during loading state.
        onView(withId(R.id.signin_promo_dismiss_button))
                .check(
                        matches(
                                allOf(
                                        not(isEnabled()),
                                        withEffectiveVisibility(Visibility.INVISIBLE))));

        // 4. Once the background sign-in flow is aborted, the loading state is cleared.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPromoCoordinator.onFlowComplete(
                                SigninAndHistorySyncCoordinator.Result.aborted()));

        onView(withId(R.id.signin_promo_primary_button))
                .check(matches(allOf(isEnabled(), withText(expectedPrimaryButtonText))));
        onView(withId(R.id.account_picker_selected_account))
                .check(matches(allOf(isDisplayed(), isEnabled())));
        if (mDelegate.canBeDismissedPermanently()) {
            onView(withId(R.id.signin_promo_dismiss_button))
                    .check(matches(allOf(isDisplayed(), isEnabled())));
        } else {
            onView(withId(R.id.signin_promo_dismiss_button))
                    .check(
                            matches(
                                    allOf(
                                            not(isEnabled()),
                                            withEffectiveVisibility(Visibility.INVISIBLE))));
        }
    }

    private void checkSigninBottomSheetStrings(@SigninAccessPoint int accessPoint) {
        signinAndOptOutHistorySyncIfNeeded(accessPoint);
        setUpSignInPromo(accessPoint);
        waitForView(withId(R.id.signin_promo_primary_button)).click();

        // Extract the config passed to the sign-in flow launcher.
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mCoordinator).startSigninFlow(configCaptor.capture());

        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();

        // Verify bottom sheet strings.
        var activity = mActivityTestRule.getActivity();
        String title = activity.getString(R.string.signin_account_picker_bottom_sheet_title);
        AccountPickerBottomSheetStrings expectedStrings;
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            int subtitleId = R.string.signin_account_picker_bottom_sheet_benefits_subtitle;
            expectedStrings =
                    new AccountPickerBottomSheetStrings.Builder(title)
                            .setSubtitleString(activity.getString(subtitleId))
                            .build();
        } else {
            expectedStrings = new AccountPickerBottomSheetStrings.Builder(title).build();
        }
        assertEquals(expectedStrings, config.bottomSheetStrings);
    }

    private void checkBookmarksAccountSettingsPromoPrimaryButtonClick() {
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
                        .expectNoRecords("Signin.SigninFunnel.BookmarkManager")
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(
                        SigninAccessPoint.BOOKMARK_MANAGER, /* hasAccounts= */ true);
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);
        waitForView(withText(R.string.sync_promo_title_bookmarks));
        waitForView(withId(R.id.signin_promo_primary_button)).click();

        verify(mOnOpenSettings).run();
        histogramWatcher.assertExpected();
        impressionHistogramWatcher.assertExpected();
    }

    private void checkLoadingStateNotShownForBookmarksSettingsPromo() {
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);
        waitForView(withText(R.string.sync_promo_title_bookmarks));

        // Before click: primary button and dismiss button are enabled.
        waitForView(
                withId(R.id.signin_promo_primary_button), withText(R.string.open_settings_button));
        onView(withId(R.id.signin_promo_dismiss_button))
                .check(matches(allOf(isDisplayed(), isEnabled())));

        onView(withId(R.id.signin_promo_primary_button)).perform(click());

        // Verify that sign-in flow is not started.
        verify(mCoordinator, never()).startSigninFlow(any());

        // Verify primary button text remains "Open settings" and promo buttons remain enabled.
        onView(withId(R.id.signin_promo_primary_button))
                .check(matches(allOf(isEnabled(), withText(R.string.open_settings_button))));
        onView(withId(R.id.signin_promo_dismiss_button))
                .check(matches(allOf(isDisplayed(), isEnabled())));
        verify(mOnOpenSettings).run();
    }

    private void checkAccountPickerClick(@SigninAccessPoint int accessPoint) {
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
                        .expectIntRecords(
                                "Signin.SigninFunnel." + getAccessPointToRenderId(accessPoint),
                                SigninFunnelEvent.PROMO_SHOWN_WITH_ACCOUNT,
                                SigninFunnelEvent.CHOOSE_OTHER_ACCOUNT_CLICKED)
                        .build();
        var impressionHistogramWatcher =
                getPromoImpressionHistogramWatcher(accessPoint, /* hasAccounts= */ true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);
        waitForView(withId(R.id.account_picker_selected_account)).click();

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

    private void checkHistoryPagePromoAccountPickerHidden() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);
        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);
        onView(withId(R.id.account_picker_selected_account))
                .check(ViewAssertions.matches(not(isDisplayed())));
    }

    private void checkBookmarksAccountSettingsPromoAccountPickerHidden() {
        var histogramWatcher =
                getPromoImpressionHistogramWatcher(
                        SigninAccessPoint.BOOKMARK_MANAGER, /* hasAccounts= */ true);
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();

        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        waitForView(withText(R.string.sync_promo_title_bookmarks));
        onView(withId(R.id.account_picker_selected_account))
                .check(ViewAssertions.matches(not(isDisplayed())));
        histogramWatcher.assertExpected();
    }

    private void checkPermanentDismissal(
            @SigninAccessPoint int accessPoint, boolean dueToUndoneSignin) {
        @IdRes int dismissButtonId = R.id.signin_promo_dismiss_button;
        String event =
                dueToUndoneSignin
                        ? SigninPromoMediator.Event.SIGNIN_UNDONE
                        : SigninPromoMediator.Event.DISMISSED;
        var histogramWatcherBuilder =
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
                                1);
        // Sign-in funnel events are only recorded for signed-out sign-in promos, whereas the
        // history page only shows a history sync promo to already signed-in users.
        if (accessPoint != SigninAccessPoint.HISTORY_PAGE) {
            if (dueToUndoneSignin) {
                histogramWatcherBuilder.expectIntRecord(
                        "Signin.SigninFunnel." + getAccessPointToRenderId(accessPoint),
                        SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT);
            } else {
                histogramWatcherBuilder.expectIntRecords(
                        "Signin.SigninFunnel." + getAccessPointToRenderId(accessPoint),
                        SigninFunnelEvent.PROMO_SHOWN_NO_ACCOUNT,
                        SigninFunnelEvent.SIGNIN_DECLINED_BUTTON_CLICKED);
            }
        } else {
            histogramWatcherBuilder.expectNoRecords("Signin.SigninFunnel.HistoryPage");
        }
        var histogramWatcher = histogramWatcherBuilder.build();
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
            waitForView(withId(dismissButtonId)).click();
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

    private void checkPromoImpressionRecorded(@SigninAccessPoint int accessPoint) {
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

    private void checkMaxImpressionReached(@SigninAccessPoint int accessPoint) {
        initNativeIfNeeded();
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

    private void checkHistoryPageImpressionDelay_firstShown() {
        signinAndOptOutHistorySyncIfNeeded(SigninAccessPoint.HISTORY_PAGE);

        setUpSignInPromo(SigninAccessPoint.HISTORY_PAGE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mPromoCoordinator.canShowPromo());
                });
    }

    private void checkHistoryPageImpressionDelay_delayNotReached() {
        initNativeIfNeeded();
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

    private void checkHistoryPageImpressionDelay_delayReached() {
        initNativeIfNeeded();
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

    private void checkHistorySyncOptIn_RecentTabs() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.SigninFunnel.RecentTabs")
                        .build();
        signinAndOptOutHistorySync();
        setUpSignInPromo(SigninAccessPoint.RECENT_TABS);
        waitForView(withId(R.id.signin_promo_primary_button)).click();

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
        histogramWatcher.assertExpected();
    }

    private void checkRendering_withAccount_bookmarksAccountSettingsPromo(boolean nightModeEnabled)
            throws Exception {
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SignedIn_"
                        + getParamToRenderId(SigninAccessPoint.BOOKMARK_MANAGER, nightModeEnabled));
    }

    private void checkRendering_noAccountThenWithAccount_compact(
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

    private void checkRendering_seamlessSigninPromo_compact_noAccount(
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

    private void checkRendering_seamlessSigninPromo_compact_withAccount(
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

    private void checkRendering_seamlessSigninPromo_compact_signedIn_bookmarks(
            boolean nightModeEnabled) throws Exception {
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "WithAccount_SeamlessSigninPromo_compact_SignedIn_"
                        + getParamToRenderId(SigninAccessPoint.BOOKMARK_MANAGER, nightModeEnabled));
    }

    private void checkRendering_seamlessSigninPromo_signedInThenSignedOut_bookmarks(
            boolean nightModeEnabled) throws Exception {
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingListDataTypes();
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedInThenSignedOut_SignedIn_"
                        + getNightModeToRenderId(nightModeEnabled));

        mSigninTestRule.signOut();

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedInThenSignedOut_SignedOut_"
                        + getNightModeToRenderId(nightModeEnabled));
    }

    private void checkRendering_seamlessSigninPromo_signedOutThenSignedIn_bookmarks(
            boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(SigninAccessPoint.BOOKMARK_MANAGER);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedOutThenSignedIn_SignedOut_"
                        + getNightModeToRenderId(nightModeEnabled));

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        mRenderTestRule.render(
                mPromoView,
                "SeamlessSigninPromo_SignedOutThenSignedIn_SignedIn_"
                        + getNightModeToRenderId(nightModeEnabled));
    }

    private void checkRendering_loadingState_Compact(boolean nightModeEnabled) throws Exception {
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

    /**
     * Runs {@code scenario} with fresh mocks, then resets all state it may have touched. Since all
     * scenarios share one test method, {@code params} is attached to any failure to tell apart the
     * parameterized runs of the same scenario method.
     */
    private void runScenario(String params, ThrowingRunnable scenario) throws Throwable {
        createMocks();
        try {
            scenario.run();
        } catch (Throwable t) {
            throw new AssertionError("Scenario failed for " + params, t);
        }
        resetState();
    }

    private void runScenario(ThrowingRunnable scenario) throws Throwable {
        runScenario("no params", scenario);
    }

    private void forEachAccessPoint(AccessPointScenario scenario) throws Throwable {
        for (@SigninAccessPoint int accessPoint : sAccessPoints) {
            runScenario(getAccessPointToRenderId(accessPoint), () -> scenario.run(accessPoint));
        }
    }

    private static void setUpNightMode(boolean nightModeEnabled) {
        // Native initialization may create the global night mode state controller, which applies
        // the default night mode from the theme setting. Keep both in sync.
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
    }

    private void forEachNightMode(NightModeScenario scenario) throws Throwable {
        for (boolean nightModeEnabled : sNightModeParams) {
            runScenario(
                    getNightModeToRenderId(nightModeEnabled),
                    () -> {
                        setUpNightMode(nightModeEnabled);
                        scenario.run(nightModeEnabled);
                    });
        }
    }

    private void forEachAccessPointAndNightMode(AccessPointNightModeScenario scenario)
            throws Throwable {
        for (@SigninAccessPoint int accessPoint : sAccessPoints) {
            for (boolean nightModeEnabled : sNightModeParams) {
                runScenario(
                        getParamToRenderId(accessPoint, nightModeEnabled),
                        () -> {
                            setUpNightMode(nightModeEnabled);
                            scenario.run(accessPoint, nightModeEnabled);
                        });
            }
        }
    }

    private void createMocks() {
        MockitoAnnotations.openMocks(this);
        lenient()
                .when(
                        mLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                any(), any(), any(), any(), any(), any(), any(), any(), any(),
                                anyInt()))
                .thenReturn(mCoordinator);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        lenient().when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        lenient().when(mExternalAuthUtilsMock.isGooglePlayServicesMissing(any())).thenReturn(false);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(
                mAccountPreviewDataServiceMock);
    }

    private void resetState() {
        if (mPromoCoordinator != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mPromoCoordinator.destroy());
            mPromoCoordinator = null;
        }
        for (String key :
                List.of(
                        ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED,
                        ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_DECLINED,
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED,
                        ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT,
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME,
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME,
                        ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME)) {
            ChromeSharedPreferences.getInstance().removeKey(key);
        }
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT);
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        // Signs out and removes all accounts. Removing the accounts also drops their account-keyed
        // sync settings (e.g. the data types disabled by disableBookmarksAndReadingListDataTypes).
        mSigninTestRule.cleanUpAccountsAndSignOut();
        // Let any Java Handler callbacks queued by the sign-out or account removal finish before
        // the next scenario inflates a new promo.
        TestThreadUtils.flushNonDelayedLooperTasks();
    }

    private void initNativeIfNeeded() {
        if (!mIsNativeInitialized) {
            NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
            mIsNativeInitialized = true;
        }
    }

    private void setUpSignInPromo(@SigninAccessPoint int accessPoint) {
        initNativeIfNeeded();

        @LayoutRes int layoutResId = SigninPromoCoordinator.getLayoutResId(accessPoint);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    Activity activity = mActivityTestRule.getActivity();
                    View promoView = LayoutInflater.from(activity).inflate(layoutResId, null);
                    LinearLayout content = new LinearLayout(activity);
                    content.setLayoutParams(
                            new LinearLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));
                    content.setGravity(Gravity.CENTER);
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
                                    mModalDialogManager,
                                    mSnackbarManager,
                                    mDeviceLockActivityLauncher,
                                    mDelegate);
                    mPromoCoordinator.setView(mPromoView);
                });
        TestThreadUtils.flushNonDelayedLooperTasks();
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
                    new NtpSigninPromoDelegate(activity, mProfile, mLauncher, mOnPromoStateChange);
            case SigninAccessPoint.RECENT_TABS ->
                    new RecentTabsSigninPromoDelegate(
                            activity, mProfile, mLauncher, mOnPromoStateChange);
            default ->
                    throw new IllegalArgumentException(
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

    private static String getNightModeToRenderId(boolean nightModeEnabled) {
        return nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled";
    }

    private static String getParamToRenderId(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) {
        return getAccessPointToRenderId(accessPoint)
                + "_"
                + getNightModeToRenderId(nightModeEnabled);
    }

    private void disableBookmarksAndReadingListDataTypes() {
        initNativeIfNeeded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.BOOKMARKS, false);
                    syncService.setSelectedType(UserSelectableType.READING_LIST, false);
                });
        assertFalse(SyncTestUtil.isBookmarksAndReadingListEnabled());
    }

    private void signinAndOptOutHistorySync() {
        initNativeIfNeeded();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.HISTORY, false);
                    syncService.setSelectedType(UserSelectableType.TABS, false);
                });
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    private void signinAndOptOutHistorySyncIfNeeded(@SigninAccessPoint int accessPoint) {
        if (accessPoint == SigninAccessPoint.HISTORY_PAGE) {
            signinAndOptOutHistorySync();
        }
    }
}
