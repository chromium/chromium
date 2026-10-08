// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link OptionalBrowsingModeButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OptionalBrowsingModeButtonControllerTest {
    /**
     * Records calls to the optional button hooks, which are no-ops in {@link ToolbarLayout} and
     * thus leave no observable state.
     */
    private static class FakeToolbarLayout extends ToolbarLayout {
        final List<ButtonData> mUpdatedButtonData = new ArrayList<>();
        int mHideCount;

        FakeToolbarLayout(Context context) {
            super(context, /* attrs= */ null);
        }

        @Override
        protected void updateOptionalButton(ButtonData buttonData) {
            mUpdatedButtonData.add(buttonData);
        }

        @Override
        protected void hideOptionalButton() {
            mHideCount++;
        }

        @Override
        protected CaptureReadinessResult isReadyForTextureCapture() {
            throw new UnsupportedOperationException();
        }

        @Override
        public LocationBar getLocationBar() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void requestKeyboardFocus() {
            throw new UnsupportedOperationException();
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock UserEducationHelper mUserEducationHelper;
    FakeToolbarLayout mToolbarLayout;
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
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mToolbarLayout = new FakeToolbarLayout(activity);
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
        assertUpdateCount(1, mNewTabButtonData);
    }

    @Test
    public void noProvidersEligible_noneShown() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        assertEquals(0, mToolbarLayout.mUpdatedButtonData.size());
    }

    @Test
    public void noProvidersEligible_oneBecomesEligible() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        assertEquals(0, mToolbarLayout.mUpdatedButtonData.size());

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        assertUpdateCount(1, mShareButtonData);
    }

    @Test
    public void higherPrecedenceBecomesEligible() {
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mVoiceButtonData);

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        assertUpdateCount(1, mShareButtonData);

        mNewTabButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        assertUpdateCount(1, mNewTabButtonData);
    }

    @Test
    public void lowerPrecedenceBecomesEligible() {
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mNewTabButtonData);

        mShareButtonData.setCanShow(true);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        assertUpdateCount(0, mShareButtonData);
        assertUpdateCount(1, mNewTabButtonData);
    }

    @Test
    public void updateCurrentlyShowingProvider() {
        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mNewTabButtonData);

        ButtonDataImpl newButtonData = mShareButtonData;
        doReturn(newButtonData).when(mButtonDataProvider1).get(mTab);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        assertUpdateCount(1, newButtonData);

        newButtonData.setCanShow(false);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, false);
        assertUpdateCount(1, mShareButtonData);
    }

    @Test
    public void updateCurrentlyNotShowingProvider() {
        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mNewTabButtonData);

        ButtonDataImpl newButtonData = mShareButtonData;
        mButtonController.buttonDataProviderChanged(mButtonDataProvider2, true);
        assertUpdateCount(0, newButtonData);

        newButtonData.setCanShow(false);
        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, false);
        assertUpdateCount(0, newButtonData);
    }

    @Test
    public void noProvidersEligible_hideCalled() {
        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mNewTabButtonData);
        mNewTabButtonData.setCanShow(false);
        mShareButtonData.setCanShow(false);
        mVoiceButtonData.setCanShow(false);

        mButtonController.updateButtonVisibility();
        assertEquals(1, mToolbarLayout.mHideCount);
    }

    @Test
    public void hintContradictsTrueValue() {
        mNewTabButtonData.setCanShow(false);
        mButtonController.updateButtonVisibility();
        assertUpdateCount(1, mShareButtonData);

        mButtonController.buttonDataProviderChanged(mButtonDataProvider1, true);
        assertUpdateCount(0, mNewTabButtonData);
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
        assertUpdateCount(1, mNewTabButtonData);
    }

    private void assertUpdateCount(int expected, ButtonData buttonData) {
        assertEquals(
                expected, Collections.frequency(mToolbarLayout.mUpdatedButtonData, buttonData));
    }

    private static ButtonDataImpl createButtonData(
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        ButtonSpec.Builder buttonSpecBuilder =
                new ButtonSpec.Builder(null, "", false).setButtonVariant(buttonVariant);
        return new ButtonDataImpl(
                /* canShow= */ true, /* isEnabled= */ true, buttonSpecBuilder.build());
    }
}
