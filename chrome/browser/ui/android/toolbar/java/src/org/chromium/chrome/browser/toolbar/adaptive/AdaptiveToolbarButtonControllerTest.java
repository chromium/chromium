// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Unit tests for the {@code MagicToolbarButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarButtonControllerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private ButtonDataProvider mShareButtonController;
    @Mock
    private ButtonDataProvider mVoiceToolbarButtonController;
    @Mock
    private Tab mTab;

    private ButtonDataImpl mButtonData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mButtonData = new ButtonDataImpl(true, null, null, 0, false, null, true);
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
    public void testDestroy_alwaysShare() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        adaptiveToolbarButtonController.destroy();

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

        verify(mShareButtonController).destroy();
        verify(mVoiceToolbarButtonController).destroy();
        verify(mVoiceToolbarButtonController).removeObserver(adaptiveToolbarButtonController);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testGet_alwaysShare() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);
        when(mShareButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertSame(mButtonData, buttonData);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    public void testGet_alwaysVoice() {
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);
        when(mVoiceToolbarButtonController.get(any())).thenReturn(mButtonData);

        AdaptiveToolbarButtonController adaptiveToolbarButtonController = buildController();
        ButtonData buttonData = adaptiveToolbarButtonController.get(mTab);

        Assert.assertSame(mButtonData, buttonData);
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
                AdaptiveToolbarButtonVariant.SHARE, mShareButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.VOICE, mVoiceToolbarButtonController);
        return adaptiveToolbarButtonController;
    }
}
