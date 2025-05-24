// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_DATA;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CONTAINER_VISIBLE;

import android.content.Context;
import android.view.View;

import androidx.core.util.Pair;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsCoordinator.HomePageButtonsState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link HomePageButtonsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomePageButtonsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private android.content.res.Resources mResources;
    @Mock private PropertyModel mModel;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private HomePageButtonView mHomeButton;

    private boolean mIsHomeButtonMenuDisabled;
    private Context mContext;
    private HomePageButtonsMediator mHomePageButtonsMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mIsHomeButtonMenuDisabled = false;
        mHomePageButtonsMediator =
                new HomePageButtonsMediator(
                        mContext,
                        mock(ObservableSupplier.class),
                        mModel,
                        (context) -> {},
                        () -> mIsHomeButtonMenuDisabled,
                        mBottomSheetController,
                        (view) -> {});
        when(mHomeButton.getRootView()).thenReturn(Mockito.mock(View.class));
        when(mHomeButton.getResources()).thenReturn(mResources);
    }

    @Test
    public void testPrepareHomePageButtonsData() {
        verify(mModel, times(2)).set(eq(BUTTON_DATA), any());

        // Verifies home button data's functionality.
        HomePageButtonData homeButtonData = mHomePageButtonsMediator.getHomeButtonDataForTesting();
        assertNotNull(homeButtonData.getOnClickListener());

        homeButtonData.getOnLongClickListener().onLongClick(mHomeButton);
        verify(mHomeButton).showMenu();
        assertEquals(1, mHomePageButtonsMediator.getMenuForTesting().size());

        mIsHomeButtonMenuDisabled = true;
        homeButtonData.getOnLongClickListener().onLongClick(mHomeButton);
        verify(mHomeButton).showMenu();

        // Verifies NTP customization data's functionality.
        HomePageButtonData ntpCustomizationButtonData =
                mHomePageButtonsMediator.getNtpCustomizationButtonDataForTesting();
        assertNull(ntpCustomizationButtonData.getOnLongClickListener());
        assertNotNull(ntpCustomizationButtonData.getOnClickListener());
    }

    @Test
    public void testUpdateButtonsState() {
        mHomePageButtonsMediator.updateButtonsState(HomePageButtonsState.HIDDEN);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(false));

        mHomePageButtonsMediator.updateButtonsState(HomePageButtonsState.SHOWING_HOME_BUTTON);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(true));
        verify(mModel).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(0, true)));
        verify(mModel).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(1, false)));

        mHomePageButtonsMediator.updateButtonsState(
                HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON);
        verify(mModel, times(2)).set(eq(IS_CONTAINER_VISIBLE), eq(true));
        verify(mModel).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(0, false)));
        verify(mModel).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(1, true)));

        mHomePageButtonsMediator.updateButtonsState(
                HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON);
        verify(mModel, times(3)).set(eq(IS_CONTAINER_VISIBLE), eq(true));
        verify(mModel, times(2)).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(0, true)));
        verify(mModel, times(2)).set(eq(IS_BUTTON_VISIBLE), eq(new Pair<>(1, true)));
    }
}
