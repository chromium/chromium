// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Render tests for the AtMemory Flyout layout. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@LargeTest
@DoNotBatch(reason = "Night mode testing requires fresh activity")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AtMemoryFlyoutViewRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private InsetObserver mInsetObserver;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    private Activity mActivity;
    private BottomSheetController mBottomSheetController;
    private AtMemoryFlyoutView mView;

    public AtMemoryFlyoutViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        sActivityTestRule.launchActivity(null);
        final AtomicReference<NonNullObservableSupplier<Integer>> supplierRef =
                new AtomicReference<>();
        runOnUiThreadBlocking(
                () -> {
                    supplierRef.set(ObservableSuppliers.alwaysZero());
                });

        when(mInsetObserver.getSupplierForKeyboardInset()).thenReturn(supplierRef.get());

        runOnUiThreadBlocking(
                () -> {
                    mActivity = sActivityTestRule.getActivity();
                    mBottomSheetController = createBottomSheetController();
                });
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private BottomSheetController createBottomSheetController() {
        ViewGroup activityContentView = mActivity.findViewById(android.R.id.content);
        activityContentView.removeAllViews();
        TypedValue typedValue = new TypedValue();
        mActivity.getTheme().resolveAttribute(R.attr.colorSurface, typedValue, true);
        activityContentView.setBackgroundColor(typedValue.data);
        ScrimManager scrimManager =
                new ScrimManager(mActivity, activityContentView, ScrimClient.NONE);
        return BottomSheetControllerFactory.createBottomSheetController(
                () -> scrimManager,
                mActivity.getWindow(),
                KeyboardVisibilityDelegate.getInstance(),
                () -> activityContentView,
                () -> 0,
                /* desktopWindowStateManager= */ null,
                mInsetObserver);
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryFlyoutView() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(AtMemoryFlyoutProperties.ALL_KEYS)
                                    .with(
                                            AtMemoryFlyoutProperties.TITLE,
                                            "Coupon code • ananas-shoe.io")
                                    .with(
                                            AtMemoryFlyoutProperties.SOURCE_TEXT,
                                            "From Gmail. Captured on May 5, 2025 in Munich,"
                                                    + " Germany")
                                    .with(
                                            AtMemoryFlyoutProperties.SUGGESTIONS,
                                            Arrays.asList(
                                                    createAutofillSuggestion("Elisa Beckett", ""),
                                                    createAutofillSuggestion(
                                                            "123530", "Passport number"),
                                                    createAutofillSuggestion(
                                                            "07-05-2032", "Issue date"),
                                                    createAutofillSuggestion(
                                                            "07-05-2032", "Expiration date"),
                                                    createAutofillSuggestion("USA", "")))
                                    .build();

                    mView = new AtMemoryFlyoutView(themeWrapper);
                    AtMemoryFlyoutContent content =
                            new AtMemoryFlyoutContent(mView.getContentView());

                    PropertyModelChangeProcessor.create(
                            model, mView, AtMemoryFlyoutViewBinder::bind);

                    mBottomSheetController.requestShowContent(content, false);
                });

        ViewUtils.waitForStableView(mView.getContentView());
        mRenderTestRule.render(mView.getContentView(), "at_memory_flyout_view");
    }

    private AutofillSuggestion createAutofillSuggestion(String label, String subLabel) {
        return new AutofillSuggestion.Builder().setLabel(label).setSubLabel(subLabel).build();
    }
}
