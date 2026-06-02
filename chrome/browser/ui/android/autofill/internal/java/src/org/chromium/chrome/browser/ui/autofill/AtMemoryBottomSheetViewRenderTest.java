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

import androidx.recyclerview.widget.RecyclerView;
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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.ViewUtils;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Render tests for the AtMemory Bottom Sheet View. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@LargeTest
@DoNotBatch(reason = "Night mode testing requires fresh activity")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AtMemoryBottomSheetViewRenderTest {
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
                    .setRevision(2)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    private Activity mActivity;
    private BottomSheetController mBottomSheetController;
    private AtMemoryBottomSheetView mView;

    public AtMemoryBottomSheetViewRenderTest(boolean nightModeEnabled) {
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
    public void testAtMemoryBottomSheetView() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(
                                    mView.getContentView(), mBottomSheetController);

                    ModelList modelList = new ModelList();
                    PropertyModel itemModel1 =
                            createSuggestionModel(
                                    "KLM204", "Flight ⋅ 15 May ⋅ SEA - MUC", R.drawable.flight);
                    modelList.add(
                            new ListItem(
                                    AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION,
                                    itemModel1));

                    PropertyModel itemModel2 =
                            createSuggestionModel(
                                    "Hotel Booking", "Hilton ⋅ 16 May", R.drawable.travel_trip);

                    modelList.add(
                            new ListItem(
                                    AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION,
                                    itemModel2));

                    PropertyModel itemModel3 =
                            createSuggestionModel(
                                    "Driving license", null, R.drawable.directions_car);

                    modelList.add(
                            new ListItem(
                                    AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION,
                                    itemModel3));

                    SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
                    adapter.registerType(
                            AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION,
                            new LayoutViewBuilder<>(
                                    R.layout.at_memory_bottom_sheet_suggestion_item),
                            AtMemoryBottomSheetSuggestionViewBinder::bind);
                    mView.setRecyclerViewAdapter(adapter);

                    mBottomSheetController.requestShowContent(content, false);
                });

        ViewUtils.waitForStableView(mView.getContentView());
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView =
                            mView.getContentView().findViewById(R.id.suggestions_view);
                    if (recyclerView.getChildCount() <= 0) {
                        throw new RuntimeException("No children in recycler view");
                    }
                });
        mRenderTestRule.render(
                mActivity.findViewById(android.R.id.content), "at_memory_bottom_sheet_view");
    }

    private static PropertyModel createSuggestionModel(
            String title, String details, int iconResId) {
        return new PropertyModel.Builder(AtMemoryBottomSheetSuggestionProperties.ALL_PROPERTIES)
                .with(AtMemoryBottomSheetSuggestionProperties.TITLE, title)
                .with(AtMemoryBottomSheetSuggestionProperties.DETAILS, details)
                .with(AtMemoryBottomSheetSuggestionProperties.ICON, iconResId)
                .build();
    }
}
