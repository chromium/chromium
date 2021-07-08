// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.app.Activity;
import android.util.Pair;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarPreferenceFragment;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for the {@link AdaptiveToolbarButtonController} */
@Config(manifest = Config.NONE,
        shadows = {ShadowRecordHistogram.class,
                AdaptiveToolbarButtonControllerTest.ShadowChromeFeatureList.class})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
@DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION)
public class AdaptiveToolbarButtonControllerTest {
    // TODO(crbug.com/1199025): Remove this shadow.
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, String> sParamValues = new HashMap<>();

        @Implementation
        public static String getFieldTrialParamByFeature(String feature, String paramKey) {
            Assert.assertTrue(ChromeFeatureList.isEnabled(feature));
            return sParamValues.getOrDefault(paramKey, "");
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private ButtonDataProvider mShareButtonController;
    @Mock
    private ButtonDataProvider mVoiceToolbarButtonController;
    @Mock
    private ButtonDataProvider mNewTabButtonController;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private Tab mTab;

    private ButtonDataImpl mButtonData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowChromeFeatureList.sParamValues.clear();
        ShadowRecordHistogram.reset();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        mButtonData = new ButtonDataImpl(
                /*canShow=*/true, /*drawable=*/null, mock(View.OnClickListener.class),
                /*contentDescriptionResId=*/0, /*supportsTinting=*/false,
                /*iphCommandBuilder=*/null, /*isEnabled=*/true);
    }

    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED);
        SharedPreferencesManager.getInstance().removeKey(ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testDestroy_alwaysNone() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_NONE);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testDestroy_alwaysNewTab() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_NEW_TAB);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mNewTabButtonController).removeObserver(adaptiveToolbarButtonController);
        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testDestroy_alwaysShare() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mShareButtonController).destroy();
        verify(mShareButtonController).removeObserver(adaptiveToolbarButtonController);
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testDestroy_alwaysVoice() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
        verify(mVoiceToolbarButtonController).removeObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testGet_alwaysShare() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.SHARE));

        when(mShareButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);
        adaptiveToolbarButtonController.destroy();

        Assert.assertEquals(101, buttonData.getButtonSpec().getContentDescriptionResId());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testGet_alwaysVoice() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.VOICE));

        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);
        adaptiveToolbarButtonController.destroy();

        Assert.assertEquals(101, buttonData.getButtonSpec().getContentDescriptionResId());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testMetrics() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.SHARE));
        when(mShareButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.AdaptiveToolbarButton.SessionVariant",
                        AdaptiveToolbarButtonVariant.SHARE));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.AdaptiveToolbarButton.SessionVariant"));

        View view = mock(View.class);
        buttonData.getButtonSpec().getOnClickListener().onClick(view);
        buttonData.getButtonSpec().getOnClickListener().onClick(view);

        Assert.assertEquals(2,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.AdaptiveToolbarButton.Clicked",
                        AdaptiveToolbarButtonVariant.SHARE));
        Assert.assertEquals(2,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.AdaptiveToolbarButton.Clicked"));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testAddObserver_alwaysShare() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.destroy();

        verify(mShareButtonController).addObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testAddObserver_alwaysVoice() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.destroy();

        verify(mVoiceToolbarButtonController).addObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testButtonDataChanged() {
        setModeParam(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        ButtonDataObserver observer = mock(ButtonDataObserver.class);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.buttonDataChanged(true);
        adaptiveToolbarButtonController.destroy();

        verify(observer).buttonDataChanged(true);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testCustomization_newTab() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.NEW_TAB));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(mNewTabButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testCustomization_share() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.SHARE));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(mShareButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testCustomization_voice() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.VOICE));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testCustomization_prefChangeTriggersButtonChange() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.VOICE));

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mActivityLifecycleDispatcher).register(adaptiveToolbarButtonController);

        ButtonDataObserver observer = mock(ButtonDataObserver.class);
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        verify(observer).buttonDataChanged(true);
        Assert.assertEquals(mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());

        SharedPreferencesManager.getInstance().writeInt(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS, AdaptiveToolbarButtonVariant.NEW_TAB);

        verify(observer, times(2)).buttonDataChanged(true);
        Assert.assertEquals(mNewTabButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testLongPress() {
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(true);
        AdaptiveToolbarStatePredictor.setSegmentationResultsForTesting(
                new Pair<>(true, AdaptiveToolbarButtonVariant.NEW_TAB));
        Activity activity = Robolectric.setupActivity(Activity.class);
        SettingsLauncher settingsLauncher = mock(SettingsLauncher.class);

        AdaptiveButtonActionMenuCoordinator menuCoordinator =
                mock(AdaptiveButtonActionMenuCoordinator.class);
        Answer<OnLongClickListener> listenerAnswer = invocation -> (view -> {
            invocation.<Callback<Integer>>getArgument(0).onResult(
                    Integer.valueOf(R.id.customize_adaptive_button_menu_id));
            return true;
        });
        doAnswer(listenerAnswer).when(menuCoordinator).createOnLongClickListener(any());

        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(activity, settingsLauncher,
                        mActivityLifecycleDispatcher, menuCoordinator,
                        SharedPreferencesManager.getInstance());
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.NEW_TAB, mNewTabButtonController);
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.NEW_TAB));
        when(mNewTabButtonController.get(any())).thenReturn(mButtonData);
        View view = mock(View.class);
        when(view.getContext()).thenReturn(activity);

        View.OnLongClickListener longClickListener =
                adaptiveToolbarButtonController.get(mTab).getButtonSpec().getOnLongClickListener();
        longClickListener.onLongClick(view);
        adaptiveToolbarButtonController.destroy();

        verify(settingsLauncher)
                .launchSettingsActivity(activity, AdaptiveToolbarPreferenceFragment.class);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
            ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
    public void
    testLegacyVoiceToolbarFeature() {
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        Assert.assertEquals(mVoiceToolbarButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION,
            ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    public void
    testLegacyShareToolbarFeature() {
        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.onFinishNativeInitialization();

        Assert.assertEquals(mShareButtonController,
                adaptiveToolbarButtonController.getSingleProviderForTesting());
    }

    private static void setModeParam(String modeValue) {
        ShadowChromeFeatureList.sParamValues.put("mode", modeValue);
    }

    private AdaptiveToolbarButtonController buildController() {
        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(mock(Activity.class),
                        mock(SettingsLauncher.class), mActivityLifecycleDispatcher,
                        mock(AdaptiveButtonActionMenuCoordinator.class),
                        SharedPreferencesManager.getInstance());
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.NEW_TAB, mNewTabButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.SHARE, mShareButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.VOICE, mVoiceToolbarButtonController);
        return adaptiveToolbarButtonController;
    }

    private static ButtonSpec makeButtonSpec(@AdaptiveToolbarButtonVariant int variant) {
        return new ButtonSpec(/*drawable=*/null, mock(View.OnClickListener.class),
                /*contentDescriptionResId=*/101, /*supportsTinting=*/false,
                /*iphCommandBuilder=*/null, variant);
    }
}
