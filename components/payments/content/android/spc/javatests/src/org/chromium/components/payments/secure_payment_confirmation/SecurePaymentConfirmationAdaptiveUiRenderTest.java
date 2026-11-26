// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Dialog;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiControllerImpl;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationController.AdaptiveUiPresenter;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/** Render tests for the Secure Payment Confirmation Adaptive UI bottom sheet and modal dialog. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class SecurePaymentConfirmationAdaptiveUiRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.BLINK_PAYMENTS)
                    .build();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final boolean mNightModeEnabled;
    private final boolean mUseRtlLayout;
    private InsetObserver mInsetObserver;
    private BottomSheetController mBottomSheetController;
    private WindowAndroid mWindowAndroid;
    private AdaptiveUiControllerImpl mAdaptiveUiController;

    public SecurePaymentConfirmationAdaptiveUiRenderTest(
            boolean nightModeEnabled, boolean useRtlLayout) {
        mNightModeEnabled = nightModeEnabled;
        mUseRtlLayout = useRtlLayout;
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @BeforeClass
    public static void setUpSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        setRtlForTesting(mUseRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(mNightModeEnabled);
        mActivityRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityRule.getActivity(), Stage.RESUMED);
        runOnUiThreadBlocking(
                () -> {
                    BlankUiTestActivity activity = getActivity();
                    mInsetObserver =
                            new InsetObserver(
                                    new ImmutableWeakReference<>(
                                            activity.getWindow().getDecorView()),
                                    new ImmutableWeakReference<>(activity.getApplicationContext()),
                                    /* enableKeyboardOverlayMode= */ false,
                                    /* enableExtraEdgeToEdgeLogging= */ false);
                    mBottomSheetController = createBottomSheetController(activity);
                    mWindowAndroid =
                            new WindowAndroid(activity, /* occlusionTrackingAllowed= */ false);
                    mAdaptiveUiController =
                            new AdaptiveUiControllerImpl(
                                    mWindowAndroid,
                                    activity.getModalDialogManager(),
                                    mBottomSheetController);
                });
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
        setRtlForTesting(false);
        runOnUiThreadBlocking(
                () -> {
                    if (mAdaptiveUiController != null) {
                        mAdaptiveUiController.destroy();
                    }
                    if (mWindowAndroid != null) {
                        mWindowAndroid.destroy();
                    }
                });
        try {
            finishActivity(getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRendersAdaptiveUiSheet() throws IOException {
        DeviceInfo.setIsDesktopForTesting(false);
        runOnUiThreadBlocking(
                () -> {
                    SecurePaymentConfirmationController controller =
                            SecurePaymentConfirmationTestUtils.createController(
                                    mWindowAndroid, /* responseCallback= */ result -> {});
                    assertThat(controller.show()).isTrue();
                    assertThat(controller.getPresenterForTesting())
                            .isInstanceOf(AdaptiveUiPresenter.class);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View sheetView = getActivity().findViewById(R.id.bottom_sheet);
        if (mUseRtlLayout) {
            runOnUiThreadBlocking(() -> sheetView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL));
        }
        CriteriaHelper.pollUiThread(
                () ->
                        sheetView.getWidth() > 0
                                && sheetView.getHeight() > 0
                                && !sheetView.isLayoutRequested());
        mRenderTestRule.render(sheetView, "spc_adaptive_ui_sheet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRendersAdaptiveUiDialog() throws IOException {
        DeviceInfo.setIsDesktopForTesting(true);
        Dialog dialog =
                runOnUiThreadBlocking(
                        () -> {
                            SecurePaymentConfirmationController controller =
                                    SecurePaymentConfirmationTestUtils.createController(
                                            mWindowAndroid, /* responseCallback= */ result -> {});
                            assertThat(controller.show()).isTrue();
                            assertThat(controller.getPresenterForTesting())
                                    .isInstanceOf(AdaptiveUiPresenter.class);
                            AppModalPresenter presenter =
                                    (AppModalPresenter)
                                            getActivity()
                                                    .getModalDialogManager()
                                                    .getCurrentPresenterForTest();
                            return Objects.requireNonNull(presenter).getDialogForTesting();
                        });
        assertThat(dialog).isNotNull();
        assertThat(dialog.getWindow()).isNotNull();
        View dialogView = dialog.getWindow().getDecorView();
        if (mUseRtlLayout) {
            // The window's first traversal resets the decor view's layout direction, so set it on
            // the content view to keep the RTL render deterministic.
            runOnUiThreadBlocking(
                    () ->
                            dialog.findViewById(android.R.id.content)
                                    .setLayoutDirection(View.LAYOUT_DIRECTION_RTL));
        }
        ViewUtils.waitForStableView(dialogView);
        mRenderTestRule.render(dialogView, "spc_adaptive_ui_dialog");
    }

    private BlankUiTestActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private BottomSheetController createBottomSheetController(BlankUiTestActivity activity) {
        ViewGroup activityContentView = activity.findViewById(android.R.id.content);
        ScrimManager scrimManager =
                new ScrimManager(activity, activityContentView, ScrimClient.NONE);
        return BottomSheetControllerFactory.createFullWidthBottomSheetController(
                () -> scrimManager,
                activity.getWindow(),
                KeyboardVisibilityDelegate.getInstance(),
                () -> activityContentView,
                mInsetObserver);
    }
}
