// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link OptionalBrowsingModeButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OptionalBrowsingModeButtonControllerTest {
    @Mock UserEducationHelper mUserEducationHelper;
    @Mock ToolbarLayout mToolbarLayout;
    @Mock ButtonDataProvider mButtonDataProvider1;
    @Mock ButtonDataProvider mButtonDataProvider2;
    @Mock ButtonDataProvider mButtonDataProvider3;
    @Mock Tab mTab;

    ButtonDataImpl mNewTabButtonData;
    ButtonDataImpl mShareButtonData;
    ButtonDataImpl mVoiceButtonData;

    @Captor ArgumentCaptor<ButtonDataProvider.ButtonDataObserver> mObserverCaptor1;
    @Captor ArgumentCaptor<ButtonDataProvider.ButtonDataObserver> mObserverCaptor2;
    @Captor ArgumentCaptor<ButtonDataProvider.ButtonDataObserver> mObserverCaptor3;

    OptionalBrowsingModeButtonController mButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mNewTabButtonData = createButtonData(AdaptiveToolbarButtonVariant.NEW_TAB);
        mShareButtonData = createButtonData(AdaptiveToolbarButtonVariant.SHARE);
        mVoiceButtonData = createButtonData(AdaptiveToolbarButtonVariant.VOICE);
        doReturn(mNewTabButtonData).when(mButtonDataProvider1).get(mTab);
        doReturn(mShareButtonData).when(mButtonDataProvider2).get(mTab);
        doReturn(mVoiceButtonData).when(mButtonDataProvider3).get(mTab);

        List<ButtonDataProvider> buttonDataProviders =
                Arrays.asList(mButtonDataProvider1, mButtonDataProvider2, mButtonDataProvider3);
        mButtonController =
                new OptionalBrowsingModeButtonController(
                        buttonDataProviders, mUserEducationHelper, mToolbarLayout, () -> mTab);
        verify(mButtonDataProvider1, times(1)).addObserver(mObserverCaptor1.capture());
        verify(mButtonDataProvider2, times(1)).addObserver(mObserverCaptor2.capture());
        verify(mButtonDataProvider3, times(1)).addObserver(mObserverCaptor3.capture());
    }

    @Test
    public void allProvidersEligible_highestPrecedenceShown() {
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);
    }

    @Test
    public void noProvidersEligible_noneShown() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(0)).updateOptionalButton(any());
    }

    @Test
    public void noProvidersEligible_oneBecomesEligible() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(0)).updateOptionalButton(any());

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        verify(mToolbarLayout, times(1)).updateOptionalButton(mShareButtonData);
    }

    @Test
    public void higherPrecedenceBecomesEligible() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mVoiceButtonData);

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        verify(mToolbarLayout, times(1)).updateOptionalButton(mShareButtonData);

        mNewTabButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);
    }

    @Test
    public void lowerPrecedenceBecomesEligible() {
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        verify(mToolbarLayout, times(0)).updateOptionalButton(mShareButtonData);
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);
    }

    @Test
    public void updateCurrentlyShowingProvider() {
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);

        ButtonDataImpl newButtonData = mShareButtonData;
        doReturn(newButtonData).when(mButtonDataProvider1).get(mTab);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        verify(mToolbarLayout, times(1)).updateOptionalButton(newButtonData);

        newButtonData.setCanShow(false);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, false);
        verify(mToolbarLayout, times(1)).updateOptionalButton(mShareButtonData);
    }

    @Test
    public void updateCurrentlyNotShowingProvider() {
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);

        ButtonDataImpl newButtonData = mShareButtonData;
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        verify(mToolbarLayout, times(0)).updateOptionalButton(newButtonData);

        newButtonData.setCanShow(false);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, false);
        verify(mToolbarLayout, times(0)).updateOptionalButton(newButtonData);
    }

    @Test
    public void noProvidersEligible_hideCalled() {
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).hideOptionalButton();
    }

    @Test
    public void hintContradictsTrueValue() {
        mNewTabButtonData.setCanShow(false);
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mShareButtonData);

        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        verify(mToolbarLayout, never()).updateOptionalButton(mNewTabButtonData);
    }

    @Test
    public void destroyRemovesObservers() {
        mButtonController.destroy();
        verify(mButtonDataProvider1, times(1)).removeObserver(mObserverCaptor1.getValue());
        verify(mButtonDataProvider2, times(1)).removeObserver(mObserverCaptor2.getValue());
        verify(mButtonDataProvider3, times(1)).removeObserver(mObserverCaptor3.getValue());
    }

    @Test
    public void updateOptionalButtonIsOnEnabled() {
        mNewTabButtonData.setEnabled(false);
        mButtonController.updateButtonVisibility();
        verify(mToolbarLayout, times(1)).updateOptionalButton(mNewTabButtonData);
    }

    private static ButtonDataImpl createButtonData(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        return new ButtonDataImpl(
                /* canShow= */ true,
                /* drawable= */ null,
                /* onClickListener= */ null,
                /* contentDescription= */ "",
                /* supportsTinting= */ false,
                /* iphCommandBuilder= */ null,
                /* isEnabled= */ true,
                buttonVariant,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);
    }
}
