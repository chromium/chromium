// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.CUSTOM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SHARE;

import android.app.Activity;
import android.app.PendingIntent;
import android.graphics.drawable.Drawable;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link GoogleBottomBarViewCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleBottomBarViewCreatorTest {

    private static final Map<Integer, Integer> BUTTON_ID_TO_CUSTOM_BUTTON_ID_MAP =
            Map.of(SAVE, 100, SHARE, 101, PIH_BASIC, 103, CUSTOM, 105);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;

    @Mock private ShareDelegate mShareDelegate;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    @Mock private PageInsightsCoordinator mPageInsightsCoordinator;
    @Mock private Supplier<PageInsightsCoordinator> mPageInsightsCoordinatorSupplier;

    private Activity mActivity;

    private BottomBarConfigCreator mConfigCreator;
    private GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        mConfigCreator = new BottomBarConfigCreator(mActivity);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    private GoogleBottomBarViewCreator getGoogleBottomBarViewCreator(
            BottomBarConfig bottomBarConfig) {
        return new GoogleBottomBarViewCreator(
                mActivity,
                mTabSupplier,
                mShareDelegateSupplier,
                mPageInsightsCoordinatorSupplier,
                bottomBarConfig);
    }

    private BottomBarConfig getEvenLayoutConfig() {
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SAVE);
        return mConfigCreator.create(buttonIdList, new ArrayList<>());
    }

    private BottomBarConfig getAllChromeButtonsConfig() {
        when(mPageInsightsCoordinatorSupplier.get()).thenReturn(mPageInsightsCoordinator);
        when(mPageInsightsCoordinatorSupplier.hasValue()).thenReturn(true);
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SAVE);
        return mConfigCreator.create(buttonIdList, new ArrayList<>());
    }

    private BottomBarConfig getAllEmbedderButtonsConfig() {
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, SAVE);

        List<CustomButtonParams> customButtonParamsList =
                List.of(
                        getMockCustomButtonParams(PIH_BASIC),
                        getMockCustomButtonParams(SHARE),
                        getMockCustomButtonParams(SAVE));

        return mConfigCreator.create(buttonIdList, customButtonParamsList);
    }

    private CustomButtonParams getMockCustomButtonParams(
            @BottomBarConfigCreator.ButtonId int buttonId) {
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
        return mConfigCreator.create(buttonIdList, new ArrayList<>());
    }

    @Test
    public void
            testCreateGoogleBottomBarView_evenLayout_logsGoogleBottomBarCreatedWithEvenLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.Created",
                        GoogleBottomBarCreatedEvent.EVEN_LAYOUT);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getEvenLayoutConfig());

        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void
            testCreateGoogleBottomBarView_spotlightLayout_logsGoogleBottomBarCreatedWithSpotlightLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.Created",
                        GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getSpotlightLayoutConfig());

        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_logsAllChromeButtonsShown() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_CHROME,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllChromeButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_logsAllEmbedderButtonsShown() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_EMBEDDER,
                                GoogleBottomBarButtonEvent.SAVE_EMBEDDER)
                        .build();
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllEmbedderButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void
            testLogButtons_pageInsightCoordinatorIsNullAndPendingIntentIsNull_logsUnknownButtons() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonShown",
                        GoogleBottomBarButtonEvent.UNKNOWN);
        List<Integer> buttonIdList = List.of(0, PIH_BASIC);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(buttonIdList, new ArrayList<>()));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_customButtonHasAssociatedCustomButtonParams_logsCustomButtons() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, CUSTOM);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                buttonIdList,
                                List.of(
                                        getMockCustomButtonParams(PIH_BASIC),
                                        getMockCustomButtonParams(ButtonId.CUSTOM))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_customButtonWithoutCustomButtonParams_doesNotLogCustomButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME)
                        .build();
        List<Integer> buttonIdList = List.of(0, PIH_BASIC, SHARE, CUSTOM);
        mGoogleBottomBarViewCreator =
                getGoogleBottomBarViewCreator(
                        mConfigCreator.create(
                                buttonIdList, List.of(getMockCustomButtonParams(PIH_BASIC))));
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testUpdateBottomBarButton_logsButtonUpdated() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonUpdated",
                        GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
        mGoogleBottomBarViewCreator = getGoogleBottomBarViewCreator(getAllEmbedderButtonsConfig());
        mGoogleBottomBarViewCreator.createGoogleBottomBarView();

        mGoogleBottomBarViewCreator.updateBottomBarButton(
                BottomBarConfigCreator.createButtonConfigFromCustomParams(
                        mActivity, getMockCustomButtonParams(SAVE)));

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }
}
