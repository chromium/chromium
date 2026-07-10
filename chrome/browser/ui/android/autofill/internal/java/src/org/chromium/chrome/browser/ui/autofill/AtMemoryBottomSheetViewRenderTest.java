// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties.TILE_DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties.TILE_ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties.TILE_TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.IS_FLYOUT_VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.TITLE;

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
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
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
                    .setRevision(5)
                    .setDescription("Change the source text UI.")
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
                mInsetObserver,
                /* enableLargeFormFactorUi= */ false);
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryBottomSheetMainScreen() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(mView, mBottomSheetController);

                    ModelList modelList = new ModelList();
                    PropertyModel itemModel1 =
                            createSuggestionModel(
                                    "KLM204",
                                    "Flight ⋅ 15 May ⋅ SEA - MUC",
                                    R.drawable.flight,
                                    /* isFlyoutVisible= */ true);
                    modelList.add(new ListItem(HomeProperties.ItemType.SUGGESTION, itemModel1));

                    PropertyModel itemModel2 =
                            createSuggestionModel(
                                    "Hotel Booking",
                                    "Hilton ⋅ 16 May",
                                    R.drawable.travel_trip,
                                    /* isFlyoutVisible= */ true);

                    modelList.add(new ListItem(HomeProperties.ItemType.SUGGESTION, itemModel2));

                    PropertyModel itemModel3 =
                            createSuggestionModel(
                                    "Driving license",
                                    null,
                                    R.drawable.directions_car,
                                    /* isFlyoutVisible= */ false);

                    modelList.add(new ListItem(HomeProperties.ItemType.SUGGESTION, itemModel3));

                    mView.getHomeView().setUpSheetItems(modelList);

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
                mActivity.findViewById(android.R.id.content), "at_memory_main_screen");
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryBottomSheetMainScreen_multilineSuggestionTitle() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(mView, mBottomSheetController);

                    ModelList modelList = new ModelList();
                    PropertyModel itemModel =
                            createSuggestionModel(
                                    "Lufthansa Flight Reservation Details confirmation code"
                                            + " ABC123XYZ",
                                    "Flight ⋅ 15 May ⋅ SEA - MUC",
                                    R.drawable.flight,
                                    /* isFlyoutVisible= */ true);
                    modelList.add(new ListItem(HomeProperties.ItemType.SUGGESTION, itemModel));

                    mView.getHomeView().setUpSheetItems(modelList);

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
                mActivity.findViewById(android.R.id.content),
                "at_memory_main_screen_multiline_title");
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryBottomSheetView_searchTile() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(mView, mBottomSheetController);

                    ModelList modelList = new ModelList();
                    PropertyModel searchTileModel =
                            createSearchTileModel(
                                    "flight",
                                    "Powered by Personal Intelligence with Gemini",
                                    R.drawable.ic_spark_24dp);
                    modelList.add(
                            new ListItem(HomeProperties.ItemType.SEARCH_TILE, searchTileModel));

                    mView.getHomeView().setUpSheetItems(modelList);
                    mView.getHomeView().setShowSuggestionsBackground(false);

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
                mActivity.findViewById(android.R.id.content),
                "at_memory_bottom_sheet_view_search_tile");
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryBottomSheetView_zeroState() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(mView, mBottomSheetController);

                    ModelList modelList = new ModelList();
                    modelList.add(
                            new ListItem(HomeProperties.ItemType.ZERO_STATE, new PropertyModel()));

                    mView.getHomeView().setUpSheetItems(modelList);
                    mView.getHomeView().setShowSuggestionsBackground(false);

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
                mActivity.findViewById(android.R.id.content),
                "at_memory_bottom_sheet_view_zero_state");
    }

    private static PropertyModel createSuggestionModel(
            String title, String details, int iconResId, boolean isFlyoutVisible) {
        return new PropertyModel.Builder(SuggestionItemProperties.ALL_KEYS)
                .with(TITLE, title)
                .with(DETAILS, details)
                .with(ICON, iconResId)
                .with(IS_FLYOUT_VISIBLE, isFlyoutVisible)
                .build();
    }

    private static PropertyModel createSearchTileModel(
            String title, String details, int iconResId) {
        return new PropertyModel.Builder(SearchItemProperties.ALL_KEYS)
                .with(TILE_TITLE, title)
                .with(TILE_DETAILS, details)
                .with(TILE_ICON, iconResId)
                .build();
    }

    @Test
    @Feature({"RenderTest"})
    public void testAtMemoryBottomSheetFlyoutScreen() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView = new AtMemoryBottomSheetView(themeWrapper);
                    AtMemoryBottomSheetContent content =
                            new AtMemoryBottomSheetContent(mView, mBottomSheetController);

                    mView.setCurrentScreen(ScreenId.FLYOUT_SCREEN);

                    PropertyModel model =
                            new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                                    .with(FlyoutProperties.TITLE, "Hotel Booking")
                                    .with(
                                            FlyoutProperties.SUGGESTIONS,
                                            List.of(
                                                    createAutofillSuggestion("Elisa Beckett", ""),
                                                    createAutofillSuggestion(
                                                            "123530", "Passport number"),
                                                    createAutofillSuggestion(
                                                            "07-05-2032", "Issue date"),
                                                    createAutofillSuggestion(
                                                            "07-05-2032", "Expiration date"),
                                                    createAutofillSuggestion("USA", "")))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model,
                            mView.getFlyoutView(),
                            AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

                    mBottomSheetController.requestShowContent(content, false);
                });

        ViewUtils.waitForStableView(mView.getContentView());
        mRenderTestRule.render(
                mActivity.findViewById(android.R.id.content), "at_memory_flyout_screen");
    }

    private AutofillSuggestion createAutofillSuggestion(String label, String subLabel) {
        return new AutofillSuggestion.Builder().setLabel(label).setSubLabel(subLabel).build();
    }
}
