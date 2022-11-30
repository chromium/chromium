// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.fre.FreUMADialogCoordinator.Listener;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;

/** Integration tests for signin FRE UMA dialog. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class FreUMADialogTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_FIRST_RUN)
                    .build();

    @Mock
    private Listener mListenerMock;

    private FreUMADialogCoordinator mCoordinator;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialogForTesting);
    }

    @Test
    @MediumTest
    public void testTurningOnAllowCrashUploadWhenCrashUploadByNotAllowedDefault() {
        showFreUMADialog(/*allowMetricsAndCrashUploading=*/false);

        onView(withId(R.id.fre_uma_dialog_switch))
                .inRoot(isDialog())
                .check(matches(not(isChecked())))
                .perform(click());
        onView(withText(org.chromium.chrome.R.string.done)).inRoot(isDialog()).perform(click());

        onView(withText(R.string.signin_fre_uma_dialog_title)).check(doesNotExist());
        verify(mListenerMock).onAllowMetricsAndCrashUploadingChecked(true);
    }

    @Test
    @MediumTest
    public void testTurningOffAllowCrashUploadWhenCrashUploadAllowedByDefault() {
        showFreUMADialog(/*allowMetricsAndCrashUploading=*/true);

        onView(withId(R.id.fre_uma_dialog_switch)).perform(click());

        onView(withText(org.chromium.chrome.R.string.done)).perform(click());
        onView(withText(R.string.signin_fre_uma_dialog_title)).check(doesNotExist());
        verify(mListenerMock).onAllowMetricsAndCrashUploadingChecked(false);
    }

    @Test
    @MediumTest
    public void testLeavingAllowCrashUploadOn() {
        showFreUMADialog(/*allowMetricsAndCrashUploading=*/true);
        onView(withId(R.id.fre_uma_dialog_switch)).check(matches(isChecked()));

        onView(withText(org.chromium.chrome.R.string.done)).perform(click());

        onView(withText(R.string.signin_fre_uma_dialog_title)).check(doesNotExist());
        verify(mListenerMock, never()).onAllowMetricsAndCrashUploadingChecked(anyBoolean());
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFreUMADialogView(boolean nightModeEnabled) throws IOException {
        showFreUMADialog(/*allowMetricsAndCrashUploading=*/true);

        CriteriaHelper.pollUiThread(() -> {
            return mCoordinator.getDialogViewForTesting()
                    .findViewById(R.id.fre_uma_dialog_dismiss_button)
                    .isShown();
        });
        mRenderTestRule.render(mCoordinator.getDialogViewForTesting(), "fre_uma_dialog");
    }

    private void showFreUMADialog(boolean allowMetricsAndCrashUploading) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final Activity activity = activityTestRule.getActivity();
            mCoordinator = new FreUMADialogCoordinator(activity,
                    new ModalDialogManager(new AppModalPresenter(activity), ModalDialogType.APP),
                    mListenerMock, allowMetricsAndCrashUploading);
        });
    }
}
