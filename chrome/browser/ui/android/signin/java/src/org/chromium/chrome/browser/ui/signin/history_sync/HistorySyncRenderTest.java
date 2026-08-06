// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.res.Configuration;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.DeviceInfo;
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
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
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
@EnableFeatures({ChromeFeatureList.ANDROID_FRE_LAYOUT_UPDATE})
@DoNotBatch(reason = "This test relies on native initialization")
public class HistorySyncRenderTest {
    /** Parameter provider for night mode, orientation, and FRE state. */
    public static class NightModeOrientationAndFreParameterProvider implements ParameterProvider {
        private static final List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ false,
                                        Configuration.ORIENTATION_PORTRAIT,
                                        /* isFre */ false)
                                .name("NightModeDisabled_Portrait_Standard"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ false,
                                        Configuration.ORIENTATION_PORTRAIT,
                                        /* isFre */ true)
                                .name("NightModeDisabled_Portrait_Centered"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ false,
                                        Configuration.ORIENTATION_LANDSCAPE,
                                        /* isFre */ false)
                                .name("NightModeDisabled_Landscape_Standard"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ false,
                                        Configuration.ORIENTATION_LANDSCAPE,
                                        /* isFre */ true)
                                .name("NightModeDisabled_Landscape_Centered"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ true,
                                        Configuration.ORIENTATION_PORTRAIT,
                                        /* isFre */ false)
                                .name("NightModeEnabled_Portrait_Standard"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ true,
                                        Configuration.ORIENTATION_PORTRAIT,
                                        /* isFre */ true)
                                .name("NightModeEnabled_Portrait_Centered"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ true,
                                        Configuration.ORIENTATION_LANDSCAPE,
                                        /* isFre */ false)
                                .name("NightModeEnabled_Landscape_Standard"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled */ true,
                                        Configuration.ORIENTATION_LANDSCAPE,
                                        /* isFre */ true)
                                .name("NightModeEnabled_Landscape_Centered"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

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
            HistorySyncRenderTest.NightModeOrientationAndFreParameterProvider.class)
    public void setupNightModeAndDeviceOrientation(
            boolean nightModeEnabled, int orientation, boolean isFre) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                (orientation == Configuration.ORIENTATION_PORTRAIT ? "Portrait" : "Landscape")
                        + (isFre ? "_Centered" : "_Standard"));
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    @After
    public void tearDown() {
        if (mHistorySyncCoordinator != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mHistorySyncCoordinator.destroy();
                        mHistorySyncCoordinator = null;
                    });
        }
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeOrientationAndFreParameterProvider.class)
    public void testHistorySyncView(boolean nightModeEnabled, int orientation, boolean isFre)
            throws IOException {
        Assume.assumeFalse(
                DeviceInfo.isDesktop() && orientation == Configuration.ORIENTATION_PORTRAIT);
        mSigninTestRule.addAccountThenSignin(TestAccounts.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(orientation, isFre);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(mHistorySyncCoordinator.getView(), "history_sync");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeOrientationAndFreParameterProvider.class)
    public void testHistorySyncViewWithMinorModeRestrictions(
            boolean nightModeEnabled, int orientation, boolean isFre) throws IOException {
        Assume.assumeFalse(
                DeviceInfo.isDesktop() && orientation == Configuration.ORIENTATION_PORTRAIT);
        mSigninTestRule.addAccountThenSignin(TestAccounts.AADC_MINOR_ACCOUNT);
        buildHistorySyncCoordinator(orientation, isFre);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(
                mHistorySyncCoordinator.getView(), "history_sync_with_minor_mode_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            HistorySyncRenderTest.NightModeOrientationAndFreParameterProvider.class)
    @EnableFeatures({ChromeFeatureList.USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION})
    public void testHistorySyncViewWithAlternateIllustration(
            boolean nightModeEnabled, int orientation, boolean isFre) throws IOException {
        Assume.assumeFalse(
                DeviceInfo.isDesktop() && orientation == Configuration.ORIENTATION_PORTRAIT);
        mSigninTestRule.addAccountThenSignin(TestAccounts.AADC_ADULT_ACCOUNT);

        buildHistorySyncCoordinator(orientation, isFre);

        onViewWaiting(withId(R.id.button_primary));
        mRenderTestRule.render(
                mHistorySyncCoordinator.getView(), "history_sync_alternate_illustration");
    }

    private void buildHistorySyncCoordinator(int orientation, boolean isFre) {
        Activity activity = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(activity, orientation);
        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            Assume.assumeTrue(
                    "Landscape layout is not supported on this device size",
                    SigninUtils.shouldShowDualPanesHorizontalLayout(activity));
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHistorySyncCoordinator =
                            new HistorySyncCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mHistorySyncDelegateMock,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    new HistorySyncConfig(
                                            mActivityTestRule
                                                    .getActivity()
                                                    .getString(R.string.history_sync_title),
                                            mActivityTestRule
                                                    .getActivity()
                                                    .getString(R.string.history_sync_subtitle)),
                                    SigninAccessPoint.WEB_SIGNIN,
                                    /* showEmailInFooter= */ false,
                                    /* shouldSignOutOnDecline= */ false,
                                    isFre,
                                    null);
                    mActivityTestRule
                            .getActivity()
                            .setContentView(mHistorySyncCoordinator.maybeRecreateView());
                });
        ViewUtils.waitForVisibleView(allOf(withId(R.id.history_sync_illustration), isDisplayed()));
    }
}
