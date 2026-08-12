// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "Night mode requires clean activity launch.")
public class EnterpriseSignalsDisclaimerRenderTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            new ChromeRenderTestRule.Builder()
                    .setCorpus(ChromeRenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(RenderTestRule.Component.ENTERPRISE)
                    .build();

    @Mock private SigninManager mSigninManager;

    private @Nullable EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    private @Nullable WindowAndroid mWindowAndroid;
    private @Nullable ViewGroup mContainer;
    private final AccountInfo mAccountInfo;
    private final boolean mUseRtlLayout;

    private static AccountInfo getAccountWithoutImage(AccountInfo accountInfo) {
        return new AccountInfo.Builder(accountInfo).accountImage(null).build();
    }

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams = getTestParams();

    // All combinations of night mode x RTL x with/out profile picture.
    private static List<ParameterSet> getTestParams() {
        List<ParameterSet> params = new ArrayList<>();
        for (boolean nightMode : new boolean[] {false, true}) {
            for (boolean rtl : new boolean[] {false, true}) {
                for (boolean withPicture : new boolean[] {true, false}) {
                    params.add(new ParameterSet().value(nightMode, rtl, withPicture));
                }
            }
        }
        return params;
    }

    public EnterpriseSignalsDisclaimerRenderTest(
            boolean nightModeEnabled, boolean useRtlLayout, boolean withProfilePicture) {
        mUseRtlLayout = useRtlLayout;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                (useRtlLayout ? "RTL_" : "LTR_") + (withProfilePicture ? "WithPic" : "NoPic"));
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mAccountInfo =
                withProfilePicture
                        ? TestAccounts.MANAGED_ACCOUNT
                        : getAccountWithoutImage(TestAccounts.MANAGED_ACCOUNT);
    }

    @Before
    @SuppressWarnings("unchecked")
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mAccountManagerTestRule.addAccount(mAccountInfo);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(mAccountInfo);

        when(mSigninManager.getIdentityManager())
                .thenReturn(mAccountManagerTestRule.getIdentityManager());
        doAnswer(
                        invocation -> {
                            ((Callback<Boolean>) invocation.getArgument(1)).onResult(true);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(any(), any());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mCoordinator != null) {
                        mCoordinator.destroy();
                        mCoordinator = null;
                    }
                    if (mWindowAndroid != null) {
                        mWindowAndroid.destroy();
                        mWindowAndroid = null;
                    }
                    mAccountManagerTestRule.removeAllAccounts();
                    NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
                });
    }

    private BottomSheetController createBottomSheetController(
            Activity activity, ViewGroup container) {
        mWindowAndroid = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true);
        InsetObserver insetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(activity.getWindow().getDecorView()),
                        new ImmutableWeakReference<>(activity.getApplicationContext()),
                        /* enableKeyboardOverlayMode= */ false,
                        /* enableExtraEdgeToEdgeLogging= */ false);
        ScrimManager scrimManager = new ScrimManager(activity, container, ScrimClient.NONE);
        return BottomSheetControllerFactory.createFullWidthBottomSheetController(
                () -> scrimManager,
                activity.getWindow(),
                mWindowAndroid.getKeyboardDelegate(),
                () -> container,
                insetObserver);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testEnterpriseSignalsDisclaimer() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    mContainer = activity.findViewById(android.R.id.content);
                    mContainer.setLayoutDirection(
                            mUseRtlLayout ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
                    mContainer.removeAllViews();
                    mCoordinator =
                            new EnterpriseSignalsDisclaimerCoordinator(
                                    activity,
                                    createBottomSheetController(activity, mContainer),
                                    mSigninManager,
                                    (url) -> {});
                    mCoordinator.show();
                });
        mRenderTestRule.render(mContainer, "enterprise_signals_disclaimer");
    }
}
