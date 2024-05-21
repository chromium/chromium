package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link GoogleBottomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleBottomBarCoordinatorTest {

    private static final Map<Integer, Integer> BUTTON_ID_TO_CUSTOM_BUTTON_ID_MAP =
            Map.of(SAVE, 100, SHARE, 101, PIH_BASIC, 103);

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
    private GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        GoogleBottomBarIntentParams googleBottomBarIntentParams =
                GoogleBottomBarIntentParams.newBuilder().build();
        List<CustomButtonParams> customButtonParamsList = new ArrayList<>();
        mGoogleBottomBarCoordinator =
                new GoogleBottomBarCoordinator(
                        mActivity,
                        mTabSupplier,
                        mShareDelegateSupplier,
                        mPageInsightsCoordinatorSupplier,
                        googleBottomBarIntentParams,
                        customButtonParamsList);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    @Test
    public void
            testCreateGoogleBottomBarView_evenLayout_logsGoogleBottomBarCreatedWithEvenLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.Created",
                        GoogleBottomBarCreatedEvent.EVEN_LAYOUT);

        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(0, PIH_BASIC, SHARE, SAVE), new ArrayList<>());
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

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

        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE), new ArrayList<>());
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void testLogButtons_beforeOnNativeInitialization_logsUnknownButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.UNKNOWN,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        when(mPageInsightsCoordinatorSupplier.hasValue()).thenReturn(true);
        when(mPageInsightsCoordinatorSupplier.get()).thenReturn(mPageInsightsCoordinator);
        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE), new ArrayList<>());
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

        mGoogleBottomBarCoordinator.getGoogleBottomBarViewCreatorForTesting().logButtons();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void
            testOnFinishNativeInitialization_pageInsightsSupplierIsPresent_logsPageInsightsChrome() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_CHROME,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        when(mPageInsightsCoordinatorSupplier.hasValue()).thenReturn(true);
        when(mPageInsightsCoordinatorSupplier.get()).thenReturn(mPageInsightsCoordinator);
        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE), new ArrayList<>());
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

        mGoogleBottomBarCoordinator.onFinishNativeInitialization();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    public void
            testOnFinishNativeInitialization_pageInsightsSupplierIsNotPresent_logsPageInsightsEmbedder() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "CustomTabs.GoogleBottomBar.ButtonShown",
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        when(mPageInsightsCoordinatorSupplier.hasValue()).thenReturn(false);
        when(mPageInsightsCoordinatorSupplier.get()).thenReturn(null);
        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE),
                        List.of(getMockCustomButtonParams(PIH_BASIC)));
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

        mGoogleBottomBarCoordinator.onFinishNativeInitialization();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    private GoogleBottomBarCoordinator createGoogleBottomBarCoordinator(
            List<Integer> encodedLayoutList, List<CustomButtonParams> customButtonParamsList) {
        GoogleBottomBarIntentParams googleBottomBarIntentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(encodedLayoutList)
                        .build();

        return new GoogleBottomBarCoordinator(
                mActivity,
                mTabSupplier,
                mShareDelegateSupplier,
                mPageInsightsCoordinatorSupplier,
                googleBottomBarIntentParams,
                customButtonParamsList);
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
}
