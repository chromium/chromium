// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Unit tests for the {@code MagicToolbarButtonController} */
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarButtonControllerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private ButtonDataProvider mShareButtonController;
    @Mock
    private ButtonDataProvider mVoiceToolbarButtonController;
    @Mock
    private ButtonDataProvider mNewTabButtonController;
    @Mock
    private Tab mTab;

    private ButtonDataImpl mButtonData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        mButtonData = new ButtonDataImpl(
                /*canShow=*/true, /*drawable=*/null, Mockito.mock(View.OnClickListener.class),
                /*contentDescriptionResId=*/0, /*supportsTinting=*/false,
                /*iphCommandBuilder=*/null, /*isEnabled=*/true);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testDestroy_alwaysNone() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testDestroy_alwaysNewTab() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NEW_TAB);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mNewTabButtonController).removeObserver(adaptiveToolbarButtonController);
        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testDestroy_alwaysShare() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mShareButtonController).destroy();
        verify(mShareButtonController).removeObserver(adaptiveToolbarButtonController);
        verify(mVoiceToolbarButtonController).destroy();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testDestroy_alwaysVoice() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

        verify(mNewTabButtonController).destroy();
        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
        verify(mVoiceToolbarButtonController).removeObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testGet_alwaysShare() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.SHARE));

        when(mShareButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertEquals(101, buttonData.getButtonSpec().getContentDescriptionResId());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testGet_alwaysVoice() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.VOICE));

        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertEquals(101, buttonData.getButtonSpec().getContentDescriptionResId());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testMetrics() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        mButtonData.setButtonSpec(makeButtonSpec(AdaptiveToolbarButtonVariant.SHARE));
        when(mShareButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.AdaptiveToolbarButton.SessionVariant",
                        AdaptiveToolbarButtonVariant.SHARE));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.AdaptiveToolbarButton.SessionVariant"));

        View view = Mockito.mock(View.class);
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
    public void testAddObserver_alwaysShare() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mShareButtonController).addObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testAddObserver_alwaysVoice() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();

        verify(mVoiceToolbarButtonController).addObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testButtonDataChanged() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        ButtonDataObserver observer = Mockito.mock(ButtonDataObserver.class);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.addObserver(observer);
        adaptiveToolbarButtonController.buttonDataChanged(true);

        verify(observer).buttonDataChanged(true);
    }

    private AdaptiveToolbarButtonController buildController() {
        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController();
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.NEW_TAB, mNewTabButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.SHARE, mShareButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.VOICE, mVoiceToolbarButtonController);
        return adaptiveToolbarButtonController;
    }

    private static ButtonSpec makeButtonSpec(@AdaptiveToolbarButtonVariant int variant) {
        return new ButtonSpec(/*drawable=*/null, Mockito.mock(View.OnClickListener.class),
                /*contentDescriptionResId=*/101, /*supportsTinting=*/false,
                /*iphCommandBuilder=*/null, variant);
    }
}
