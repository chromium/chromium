// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
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

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;
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
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.ArrayList;
import java.util.List;

@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class SigninPromoCoordinatorTest {
    private static final List<Integer> sAccessPoints =
            List.of(
                    SigninAccessPoint.BOOKMARK_MANAGER,
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
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

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

    private PersonalizedSigninPromoView mPromoView;
    private SigninPromoCoordinator mPromoCoordinator;
    private SigninPromoDelegate mDelegate;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @ParameterAnnotations.UseMethodParameterBefore(RenderTestParams.class)
    public void setUpRenderTest(@SigninAccessPoint int accessPoint, boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testPrimaryButtonClick(@SigninAccessPoint int accessPoint) {
        setUpSignInPromo(accessPoint);
        onView(withId(R.id.sync_promo_signin_button)).perform(click());

        @HistorySyncConfig.OptInMode
        int historyOptInMode =
                accessPoint == SigninAccessPoint.RECENT_TABS
                        ? HistorySyncConfig.OptInMode.REQUIRED
                        : HistorySyncConfig.OptInMode.NONE;
        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityTestRule.getActivity()),
                        eq(mProfile),
                        any(AccountPickerBottomSheetStrings.class),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode
                                        .BOTTOM_SHEET),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(historyOptInMode),
                        eq(accessPoint),
                        isNull());
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(AccessPointParams.class)
    public void testSecondaryButtonClick(@SigninAccessPoint int accessPoint) {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        if (accessPoint == SigninAccessPoint.RECENT_TABS) {
            onView(withId(R.id.sync_promo_choose_account_button))
                    .check(ViewAssertions.matches(not(isDisplayed())));
            return;
        }
        onView(withId(R.id.sync_promo_choose_account_button)).perform(click());

        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivityTestRule.getActivity()),
                        eq(mProfile),
                        any(AccountPickerBottomSheetStrings.class),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode
                                        .BOTTOM_SHEET),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .CHOOSE_ACCOUNT_BOTTOM_SHEET),
                        eq(HistorySyncConfig.OptInMode.NONE),
                        eq(accessPoint),
                        isNull());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(RenderTestParams.class)
    public void testRendering_noAccount(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) throws Exception {
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
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setUpSignInPromo(accessPoint);

        mRenderTestRule.render(
                mPromoView, "WithAccount_" + getParamToRenderId(accessPoint, nightModeEnabled));
    }

    private void setUpSignInPromo(@SigninAccessPoint int accessPoint) {
        // Load native to have access to profile.
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        @LayoutRes int layoutResId = SigninPromoCoordinator.getLayoutResId(accessPoint);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
                    mDelegate = getSigninPromoDelegate(accessPoint, activity, mProfile, mLauncher);
                    mPromoCoordinator =
                            new SigninPromoCoordinator(
                                    activity,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    mDelegate);
                    mPromoCoordinator.setView(mPromoView);
                });
    }

    private static SigninPromoDelegate getSigninPromoDelegate(
            @SigninAccessPoint int accessPoint,
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> SigninPromoDelegate.forBookmarkManager(
                    context, profile, launcher);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> SigninPromoDelegate.forNtpFeedTopPromo(
                    context, profile, launcher);
            case SigninAccessPoint.RECENT_TABS -> SigninPromoDelegate.forRecentTabs(
                    context, profile, launcher);
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getAccessPointToRenderId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> "BookmarkManager";
            case SigninAccessPoint.NTP_FEED_TOP_PROMO -> "NtpFeedTopPromo";
            case SigninAccessPoint.RECENT_TABS -> "RecentTabs";
            default -> throw new IllegalArgumentException("Invalid sign-in promo access point");
        };
    }

    private static String getParamToRenderId(
            @SigninAccessPoint int accessPoint, boolean nightModeEnabled) {
        String head = getAccessPointToRenderId(accessPoint);
        String tail = nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled";
        return head + "_" + tail;
    }
}
