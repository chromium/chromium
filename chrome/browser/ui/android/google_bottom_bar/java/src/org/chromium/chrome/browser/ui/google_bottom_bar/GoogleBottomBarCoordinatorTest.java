// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.CUSTOM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.HOME;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SEARCH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SHARE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BOTTOM_BAR_CREATED_HISTOGRAM;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_SHOWN_HISTOGRAM;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarCreatedEvent;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams.VariantLayoutType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link GoogleBottomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleBottomBarCoordinatorTest {

    private static final Map<Integer, Integer> BUTTON_ID_TO_CUSTOM_BUTTON_ID_MAP =
            Map.of(SAVE, 100, SHARE, 101, PIH_BASIC, 103, CUSTOM, 105, SEARCH, 106, HOME, 107);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;

    @Mock private ShareDelegate mShareDelegate;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Profile mProfile;

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
                        googleBottomBarIntentParams,
                        customButtonParamsList);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
    }

    @Test
    public void
            testCreateGoogleBottomBarView_evenLayout_logsGoogleBottomBarCreatedWithEvenLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.EVEN_LAYOUT);

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
                        BOTTOM_BAR_CREATED_HISTOGRAM, GoogleBottomBarCreatedEvent.SPOTLIGHT_LAYOUT);

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
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.UNKNOWN,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
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
            testOnFinishNativeInitialization_pageInsightsSupplierIsNotPresent_logsPageInsightsEmbedder() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                BUTTON_SHOWN_HISTOGRAM,
                                GoogleBottomBarButtonEvent.PIH_EMBEDDER,
                                GoogleBottomBarButtonEvent.SHARE_CHROME,
                                GoogleBottomBarButtonEvent.SAVE_DISABLED)
                        .build();
        mGoogleBottomBarCoordinator =
                createGoogleBottomBarCoordinator(
                        List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE),
                        List.of(getMockCustomButtonParams(PIH_BASIC)));
        mGoogleBottomBarCoordinator.createGoogleBottomBarView();

        mGoogleBottomBarCoordinator.onFinishNativeInitialization();

        histogramWatcher.assertExpected();
        histogramWatcher.close();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabled_doesNotCheckDefaultSearchEngine() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mGoogleBottomBarCoordinator.initDefaultSearchEngine(mProfile);

        verifyNoInteractions(mTemplateUrlService);
        // Verify that true value is not written to Shared preferences
        // Use true as default value as we expect to get false
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabledAndNotEqualToGoogle_writesFalse() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, true);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);

        mGoogleBottomBarCoordinator.initDefaultSearchEngine(mProfile);

        // Use true as default value as we expect to get false
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            initDefaultSearchEngine_variantLayoutsEnabled_defaultSearchEngineCheckEnabledAndEqualsToGoogle_writesTrue() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mGoogleBottomBarCoordinator.initDefaultSearchEngine(mProfile);

        // Use false as default value as we expect to get true
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, /* defaultValue= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsDisabled_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_noVariantLayout_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.NO_VARIANT)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_doubleDecker_returnsSetWithAllSupportedCustomParams() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.DOUBLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_singleDecker_returnsEmptySet() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(Set.of(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabledButNotSupported_singleDecker_returnsSetWithAllSupportedCustomParams() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, SHARE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabled_singleDeckerWithButtonsOnRight_returnsSetWithEncodedButtons() {
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(Set.of(100, 105), supportedGoogleBottomBarButtons);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS)
    public void
            getSupportedCustomButtonParamIds_variantLayoutsEnabledButNotSupported_singleDeckerWithButtonsOnRight_returnsSetWithAllSupportedCustomParams() {
        IS_GOOGLE_DEFAULT_SEARCH_ENGINE_CHECK_ENABLED.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(IS_CHROME_DEFAULT_SEARCH_ENGINE_GOOGLE, false);
        GoogleBottomBarIntentParams intentParams =
                GoogleBottomBarIntentParams.newBuilder()
                        .addAllEncodedButton(List.of(0, SAVE, CUSTOM))
                        .setVariantLayoutType(VariantLayoutType.SINGLE_DECKER_WITH_RIGHT_BUTTONS)
                        .build();

        Set<Integer> supportedGoogleBottomBarButtons =
                GoogleBottomBarCoordinator.getSupportedCustomButtonParamIds(intentParams);

        assertEquals(
                CUSTOM_BUTTON_PARAM_ID_TO_BUTTON_ID_MAP.keySet(), supportedGoogleBottomBarButtons);
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
                googleBottomBarIntentParams,
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
}
