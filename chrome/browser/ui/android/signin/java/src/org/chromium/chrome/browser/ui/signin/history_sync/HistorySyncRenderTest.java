// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Tests for the standalone history sync consent dialog */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION})
@DoNotBatch(reason = "This test relies on native initialization")
public class HistorySyncRenderTest {
    /** Parameter provider for night mode state and device orientation. */
    public static class NightModeAndOrientationParameterProvider implements ParameterProvider {
        private static List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ false,
                                        Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeDisabled_Portrait"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ false,
                                        Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeDisabled_Landscape"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ true,
                                        Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeEnabled_Portrait"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ true,
                                        Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeEnabled_Landscape"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.SERVICES_SIGN_IN)
                    .setRevision(3)
                    .setDescription("Update button stacking")
                    .build();

    @Mock private SyncService mSyncServiceMock;
    @Mock private HistorySyncCoordinator.HistorySyncDelegate mHistorySyncDelegateMock;

    private HistorySyncCoordinator mHistorySyncCoordinator;

    @ParameterAnnotations.UseMethodParameterBefore(
            HistorySyncRenderTest.NightModeAndOrientationParameterProvider.class)
    public void setupNightModeAndDeviceOrientation(boolean nightModeEnabled, int orientation) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                orientation == Configuration.ORIENTATION_PORTRAIT ? "Portrait" : "Landscape");
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeAndOrientationParameterProvider.class)
    public void testHistorySyncView(boolean nightModeEnabled, int orientation) throws IOException {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(orientation);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(mHistorySyncCoordinator.getView(), "history_sync");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeAndOrientationParameterProvider.class)
    public void testHistorySyncViewWithMinorModeRestrictions(
            boolean nightModeEnabled, int orientation) throws IOException {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        buildHistorySyncCoordinator(orientation);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(
                mHistorySyncCoordinator.getView(), "history_sync_with_minor_mode_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeAndOrientationParameterProvider.class)
    @EnableFeatures({ChromeFeatureList.USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION})
    public void testHistorySyncViewWithAlternateIllustration(
            boolean nightModeEnabled, int orientation) throws IOException {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(orientation);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(
                mHistorySyncCoordinator.getView(), "history_sync_alternate_illustration");
    }

    private void buildHistorySyncCoordinator(int orientation) {
        ActivityTestUtils.rotateActivityToOrientation(mActivityTestRule.getActivity(), orientation);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncCoordinator =
                            new HistorySyncCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mHistorySyncDelegateMock,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    SigninAccessPoint.UNKNOWN,
                                    /* showEmailInFooter= */ false,
                                    /* signOutOnDecline= */ false,
                                    null);
                    mActivityTestRule
                            .getActivity()
                            .setContentView(mHistorySyncCoordinator.maybeRecreateView());
                });
        ViewUtils.waitForVisibleView(allOf(withId(R.id.history_sync_illustration), isDisplayed()));
    }
}
