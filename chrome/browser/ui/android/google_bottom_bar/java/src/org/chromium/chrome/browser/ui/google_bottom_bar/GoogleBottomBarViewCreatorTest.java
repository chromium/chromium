// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.CUSTOM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.HOME;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SEARCH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SHARE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_CREATED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_SHOWN_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_UPDATED_HISTOGRAM;

import android.app.Activity;
import android.app.PendingIntent;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarVariantCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams.VariantLayoutType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link GoogleBottomBarViewCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleBottomBarViewCreatorTest {
    private static final Map<Integer, Integer> BUTTON_ID_TO_CUSTOM_BUTTON_ID_MAP =
            Map.of(SAVE, 100, SHARE, 101, PIH_BASIC, 103, CUSTOM, 105, SEARCH, 106, HOME, 107);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;

    @Mock private ShareDelegate mShareDelegate;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    private Activity mActivity;

    private BottomBarConfigCreator mConfigCreator;
    private GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;

    private HistogramWatcher mHistogramWatcher;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        mConfigCreator = new BottomBarConfigCreator(mActivity);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    @After
    public void tearDown() {
        if (mHistogramWatcher != null) {
            mHistogramWatcher.assertExpected();
            mHistogramWatcher.close();
            mHistogramWatcher = null;
        }
    }

    private GoogleBottomBarViewCreator getGoogleBottomBarViewCreator(
            BottomBarConfig bottomBarConfig) {
        return new GoogleBottomBarViewCreator(
                mActivity, mTabSupplier, mShareDelegateSupplier, bottomBarConfig);
    }

    private BottomBarConfig getEvenLayoutConfig() {
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SAVE);
        return mConfigCreator.create(
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build(),
                new ArrayList<>());
    }

    private BottomBarConfig getAllChromeButtonsConfig() {
        return getAllChromeButtonsConfig(List.of(0, SHARE, SAVE));
    }

    private BottomBarConfig getAllChromeButtonsConfig(List<Integer> buttonIdList) {
        return mConfigCreator.create(
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build(),
                new ArrayList<>());
    }

    private BottomBarConfig getAllEmbedderButtonsConfig() {
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SAVE);

        List<CustomButtonParams> customButtonParamsList =
                List.of(
                        getMockCustomButtonParams(PIH_BASIC),
                        getMockCustomButtonParams(SHARE),
                        getMockCustomButtonParams(SAVE));

        return mConfigCreator.create(
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build(),
                customButtonParamsList);
    }

    private CustomButtonParams getMockCustomButtonParams(@ButtonId int buttonId) {
        CustomButtonParams customButtonParams = mock(CustomButtonParams.class);
        Drawable drawable = mock(Drawable.class);
        when(drawable.mutate()).thenReturn(drawable);

        when(customButtonParams.getId())
                .thenReturn(BUTTON_ID_TO_CUSTOM_BUTTON_ID_MAP.get(buttonId));
        when(customButtonParams.getIcon(mActivity)).thenReturn(drawable);
        when(customButtonParams.getDescription()).thenReturn("Description");
        when(customButtonParams.getPendingIntent()).thenReturn(mock(PendingIntent.class));

        return customButtonParams;
    }

    private BottomBarConfig getSpotlightLayoutConfig() {
        List<Integer> buttonIdList = List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE);
        return mConfigCreator.create(
                GoogleBottomBarIntentParams.newBuilder().addAllEncodedButton(buttonIdList).build(),
                new ArrayList<>());
    }

    @Test
    public void
            testCreateGoogleBottomBarView_evenLayout_logsGoogleBottomBarCreatedWithEvenLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.EVEN_LAYOUT);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getEvenLayoutConfig());

        mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    @Test
    public void
            testCreateGoogleBottomBarView_spotlightLayout_logsGoogleBottomBarCreatedWithSpotlightLayout() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getSpotlightLayoutConfig());

        mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    @Test
    public void
            testCreateGoogleBottomBarView_noVariantLayout_returnsLayoutWithBottomBarButtonsContainer() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                        GoogleBottomBarVariantCreatedEvent.NO_VARIANT);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getEvenLayoutConfig());

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertNotNull(view.findViewById(R.id.bottom_bar_buttons_container));
        assertNull(view.findViewById(R.id.bottom_bar_searchbox_container));
        assertNull(view.findViewById(R.id.bottom_bar_buttons_on_right_container));
        assertNotNull(view.findViewById(SHARE));
        assertNotNull(view.findViewById(SAVE));
        assertNotNull(view.findViewById(PIH_BASIC));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            testCreateGoogleBottomBarView_doubleDeckerLayout_returnsLayoutWithBottomBarButtonsAndSearchboxContainers() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                                GoogleBottomBarVariantCreatedEvent.DOUBLE_DECKER)
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.SEARCHBOX_HOME,
                                GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_LENS)
                        .build();
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of(0, SHARE, SAVE))
                                .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                                .build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertNotNull(view.findViewById(R.id.bottom_bar_buttons_container));
        assertNotNull(view.findViewById(R.id.bottom_bar_searchbox_container));
        assertNull(view.findViewById(R.id.bottom_bar_buttons_on_right_container));
        assertNotNull(view.findViewById(R.id.google_bottom_bar_searchbox));
        assertNotNull(view.findViewById(SHARE));
        assertNotNull(view.findViewById(SAVE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            testCreateGoogleBottomBarView_singleDeckerLayout_returnsLayoutWithBottomBarSearchboxContainer() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                                GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER)
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.SEARCHBOX_HOME,
                                GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_LENS)
                        .build();
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of())
                                .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                                .build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertNull(view.findViewById(R.id.bottom_bar_buttons_container));
        assertNotNull(view.findViewById(R.id.bottom_bar_searchbox_container));
        assertNull(view.findViewById(R.id.bottom_bar_buttons_on_right_container));
        assertNotNull(view.findViewById(R.id.google_bottom_bar_searchbox));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            testCreateGoogleBottomBarView_singleDeckerLayout_heightAtLeast60_setsPaddingWithLargeTop() {
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of())
                                .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                                .setSingleDeckerHeightDp(60)
                                .build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_searchbox_horizontal_padding),
                view.getPaddingStart());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_searchbox_horizontal_padding),
                view.getPaddingEnd());
        assertEquals(0, view.getPaddingBottom());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_single_decker_top_padding_large),
                view.getPaddingTop());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            testCreateGoogleBottomBarView_singleDeckerLayout_heightLessThan60_setsPaddingWithSmallTop() {
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of())
                                .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                                .setSingleDeckerHeightDp(59)
                                .build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_searchbox_horizontal_padding),
                view.getPaddingStart());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_searchbox_horizontal_padding),
                view.getPaddingEnd());
        assertEquals(0, view.getPaddingBottom());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.google_bottom_bar_single_decker_top_padding_small),
                view.getPaddingTop());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            testCreateGoogleBottomBarView_singleDeckerWithRightButtonsLayout_returnsLayoutWithBottomBarButtonsOnRightAndSearchboxContainers() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                BOTTOM_BAR_VARIANT_CREATED_HISTOGRAM,
                                GoogleBottomBarVariantCreatedEvent.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.SEARCHBOX_HOME,
                                GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH,
                                GoogleBottomBarButtonEvent.SEARCHBOX_LENS)
                        .build();
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of(0, SHARE))
                                .setVariantLayoutType(
                                        VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                                .build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        View view = mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        assertNull(view.findViewById(R.id.bottom_bar_buttons_container));
        assertNotNull(view.findViewById(R.id.bottom_bar_searchbox_container));
        assertNotNull(view.findViewById(R.id.bottom_bar_buttons_on_right_container));
        assertNotNull(view.findViewById(SHARE));
        assertNotNull(view.findViewById(R.id.google_bottom_bar_searchbox));
    }

    @Test
    public void testLogButtons_logsAllChromeButtonsShown() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllChromeButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_logsAllEmbedderButtonsShown() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_EMBEDDER,
                                GoogleBottomBarButtonEvent.SAVE_EMBEDDER)
                        .build();
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllEmbedderButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_pageInsightsPendingIntentIsNull_logsUnknownButtons() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_SHOWN_HISTOGRAM, GoogleBottomBarButtonEvent.UNKNOWN);
        List<Integer> buttonIdList = List.of(0, PIH_BASIC);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                new ArrayList<>()));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_customButtonHasAssociatedCustomButtonParams_logsCustomButtons() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, CUSTOM);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(
                                        getMockCustomButtonParams(PIH_BASIC),
                                        getMockCustomButtonParams(CUSTOM))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_customButtonWithoutCustomButtonParams_doesNotLogCustomButton() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, CUSTOM);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(getMockCustomButtonParams(PIH_BASIC))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void
            testLogButtons_searchButtonHasAssociatedCustomButtonParams_logsSearchEmbedderButton() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SEARCH_EMBEDDER)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SEARCH);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(
                                        getMockCustomButtonParams(PIH_BASIC),
                                        getMockCustomButtonParams(SEARCH))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_searchButtonWithoutCustomButtonParams_logsSearchChromeButton() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SEARCH_CHROME)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SEARCH);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(getMockCustomButtonParams(PIH_BASIC))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();
    }

    @Test
    public void testLogButtons_homeButtonHasAssociatedCustomButtonParams_logsHomeEmbedderButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.HOME_EMBEDDER)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, HOME);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(
                                        getMockCustomButtonParams(PIH_BASIC),
                                        getMockCustomButtonParams(HOME))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_homeButtonWithoutCustomButtonParams_logsHomeChromeButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.HOME_CHROME)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, HOME);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                GoogleBottomBarIntentParams.newBuilder()
                                        .addAllEncodedButton(buttonIdList)
                                        .build(),
                                List.of(getMockCustomButtonParams(PIH_BASIC))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testUpdateBottomBarButton_logsButtonUpdated() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_UPDATED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllEmbedderButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.updateBottomBarButton(
                BottomBarConfigCreator.createButtonConfigFromCustomParams(
                        mActivity, getMockCustomButtonParams(SAVE)));
    }

    @Test
    public void testBottomBarWithEligibleEvenConfig_googleBottomBarButtonsCreated() {
        BottomBarConfig config = getAllChromeButtonsConfig();
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(config);
        ViewGroup rootView = (ViewGroup) mGoogleBottomBarViewCreator.createGoogleBottomBarView();
        assertButtonLayoutCreated(config, (ViewGroup) rootView.getChildAt(0));
    }

    @Test
    public void testBottomBarWithEligibleSpotlightConfig_googleBottomBarButtonsCreated() {
        BottomBarConfig config =
                getAllChromeButtonsConfig(List.of(PIH_BASIC, SHARE, PIH_BASIC, SAVE));
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(config);
        ViewGroup rootView = (ViewGroup) mGoogleBottomBarViewCreator.createGoogleBottomBarView();
        assertButtonLayoutCreated(config, (ViewGroup) rootView.getChildAt(0));
    }

    @Test
    public void testGetBottomBarHeightInPx_returnsHeightFromConfig() {
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder().setNoVariantHeightDp(123).build(),
                        new ArrayList<>());
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(bottomBarConfig);

        assertEquals(
                ViewUtils.dpToPx(mActivity, (float) 123),
                mGoogleBottomBarViewCreator.getBottomBarHeightInPx());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void testCreateSearchBoxView_returnsViewWithSearcboxElements() {
        BottomBarConfig bottomBarConfig =
                mConfigCreator.create(
                        GoogleBottomBarIntentParams.newBuilder()
                                .addAllEncodedButton(List.of(0, SHARE))
                                .setVariantLayoutType(
                                        VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                                .build(),
                        new ArrayList<>());

        View root = getGoogleBottomBarViewCreator(bottomBarConfig).createGoogleBottomBarView();
        View superGButton = root.findViewById(R.id.google_bottom_bar_searchbox_super_g_button);
        View hintTextView = root.findViewById(R.id.google_bottom_bar_searchbox_mic_button);
        View micButton = root.findViewById(R.id.google_bottom_bar_searchbox_mic_button);
        View lensButton = root.findViewById(R.id.google_bottom_bar_searchbox_lens_button);
        assertTrue(superGButton.hasOnClickListeners());
        assertTrue(hintTextView.hasOnClickListeners());
        assertTrue(micButton.hasOnClickListeners());
        assertTrue(lensButton.hasOnClickListeners());
    }

    private void assertButtonLayoutCreated(BottomBarConfig config, ViewGroup root) {
        if (config.getSpotlightId() != null) {
            assertButtonSpotlightLayoutCreated(config, root);
        } else {
            assertButtonEvenLayoutCreated(config, root);
        }
    }

    private void assertButtonEvenLayoutCreated(BottomBarConfig config, ViewGroup root) {
        List<BottomBarConfig.ButtonConfig> buttonList = config.getButtonList();
        int totalButtonCount = buttonList.size();

        assertEquals(root.getChildCount(), totalButtonCount);
        for (int index = 0; index < totalButtonCount; index++) {
            assertButtonCreated(
                    (ImageButton) root.getChildAt(/* index= */ index), buttonList.get(index));
        }
    }

    private void assertButtonSpotlightLayoutCreated(BottomBarConfig config, ViewGroup root) {
        @ButtonId int spotlightId = config.getSpotlightId();
        assertButtonCreated(
                (ImageButton) root.getChildAt(/* index= */ 0),
                findButtonConfig(spotlightId, config));

        ViewGroup nonSpotlitContainer =
                root.findViewById(R.id.google_bottom_bar_non_spotlit_buttons_container);
        assertNotNull(nonSpotlitContainer);

        List<BottomBarConfig.ButtonConfig> buttonList = config.getButtonList();
        buttonList.remove(spotlightId);
        int totalButtonCount = buttonList.size();

        assertEquals(nonSpotlitContainer.getChildCount(), totalButtonCount);
        for (int index = 0; index < totalButtonCount; index++) {
            assertButtonCreated(
                    (ImageButton) nonSpotlitContainer.getChildAt(/* index= */ index),
                    buttonList.get(index));
        }
    }

    private void assertButtonCreated(
            ImageButton button, BottomBarConfig.ButtonConfig buttonConfig) {
        assertEquals(button.getId(), buttonConfig.getId());
        assertEquals(button.getVisibility(), View.VISIBLE);
        assertEquals(button.getContentDescription(), buttonConfig.getDescription());
        assertEquals(button.getDrawable(), buttonConfig.getIcon());
        assertTrue(button.hasOnClickListeners());
    }

    private @Nullable BottomBarConfig.ButtonConfig findButtonConfig(
            @ButtonId int buttonId, BottomBarConfig baseConfig) {
        return baseConfig.getButtonList().stream()
                .filter(config -> config.getId() == buttonId)
                .findFirst()
                .orElse(null);
    }
}
