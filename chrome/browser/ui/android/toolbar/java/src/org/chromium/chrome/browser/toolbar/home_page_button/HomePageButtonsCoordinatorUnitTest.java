// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsCoordinator.HomePageButtonsState;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link HomePageButtonsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class HomePageButtonsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private View.OnClickListener mOnHomeButtonClickListener;
    @Mock private HomePageButtonsContainerView mView;
    @Mock private HomePageButtonsMediator mMediator;
    @Mock private PropertyModel mModel;
    @Mock private ColorStateList mColorStateList;

    private Context mContext;
    private HomePageButtonsCoordinator mHomePageButtonsCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mHomePageButtonsCoordinator =
                new HomePageButtonsCoordinator(
                        mContext,
                        mock(ObservableSupplier.class),
                        mView,
                        mock(Callback.class),
                        mock(Supplier.class),
                        mBottomSheetController,
                        mOnHomeButtonClickListener);
        mHomePageButtonsCoordinator.setMediatorForTesting(mMediator);
        mHomePageButtonsCoordinator.setModelForTesting(mModel);
    }

    @Test
    public void testUpdateButtonsState() {
        mHomePageButtonsCoordinator.setHomePageButtonsStateForTesting(HomePageButtonsState.HIDDEN);
        mHomePageButtonsCoordinator.updateButtonsState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ true);
        verify(mMediator)
                .updateButtonsState(
                        HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.updateButtonsState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ false,
                /* isHomepageNonNtp= */ true);
        verify(mMediator).updateButtonsState(HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.setHomePageButtonsStateForTesting(HomePageButtonsState.HIDDEN);
        mHomePageButtonsCoordinator.updateButtonsState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false);
        verify(mMediator, times(2))
                .updateButtonsState(HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.updateButtonsState(
                /* toolbarVisualState= */ VisualState.BRAND_COLOR,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false);
        verify(mMediator).updateButtonsState(HomePageButtonsState.SHOWING_HOME_BUTTON);

        mHomePageButtonsCoordinator.updateButtonsState(
                /* toolbarVisualState= */ VisualState.BRAND_COLOR,
                /* isHomeButtonEnabled= */ false,
                /* isHomepageNonNtp= */ false);
        verify(mMediator).updateButtonsState(HomePageButtonsState.HIDDEN);
    }

    @Test
    public void testSetButtonsForegroundColor() {
        mHomePageButtonsCoordinator.setButtonsForegroundColor(mColorStateList);
        verify(mModel).set(eq(BUTTON_TINT_LIST), eq(mColorStateList));
    }

    @Test
    public void testSetButtonsBackgroundResource() {
        mHomePageButtonsCoordinator.setButtonsBackgroundResource(
                R.drawable.default_icon_background);
        verify(mModel).set(eq(BUTTON_BACKGROUND), eq(R.drawable.default_icon_background));
    }
}
