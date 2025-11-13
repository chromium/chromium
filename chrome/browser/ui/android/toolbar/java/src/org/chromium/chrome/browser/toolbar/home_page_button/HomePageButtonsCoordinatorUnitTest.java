// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ACCESSIBILITY_TRAVERSAL_BEFORE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.CONTAINER_VISIBILITY;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ON_KEY_LISTENER;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.TRANSLATION_Y;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.IdRes;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsCoordinator.HomePageButtonsState;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

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
    @Mock private View.OnKeyListener mOnKeyListener;

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
        mHomePageButtonsCoordinator.updateState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ true,
                /* urlHasFocus= */ true);
        verify(mMediator)
                .updateButtonsState(
                        HomePageButtonsState.SHOWING_BOTH_HOME_AND_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.updateState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ false,
                /* isHomepageNonNtp= */ true,
                /* urlHasFocus= */ true);
        verify(mMediator).updateButtonsState(HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.setHomePageButtonsStateForTesting(HomePageButtonsState.HIDDEN);
        mHomePageButtonsCoordinator.updateState(
                /* toolbarVisualState= */ VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ true);
        verify(mMediator, times(2))
                .updateButtonsState(HomePageButtonsState.SHOWING_CUSTOMIZATION_BUTTON);

        mHomePageButtonsCoordinator.updateState(
                /* toolbarVisualState= */ VisualState.BRAND_COLOR,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ true);
        verify(mMediator).updateButtonsState(HomePageButtonsState.SHOWING_HOME_BUTTON);

        mHomePageButtonsCoordinator.updateState(
                /* toolbarVisualState= */ VisualState.BRAND_COLOR,
                /* isHomeButtonEnabled= */ false,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ true);
        verify(mMediator).updateButtonsState(HomePageButtonsState.HIDDEN);
    }

    @Test
    public void testOnTintChanged() {
        mHomePageButtonsCoordinator.onTintChanged(
                mColorStateList, mColorStateList, BrandedColorScheme.APP_DEFAULT);
        verify(mModel).set(eq(BUTTON_TINT_LIST), eq(mColorStateList));
    }

    @Test
    public void testSetBackgroundResource() {
        mHomePageButtonsCoordinator.setBackgroundResource(R.drawable.default_icon_background);
        verify(mModel).set(eq(BUTTON_BACKGROUND), eq(R.drawable.default_icon_background));
    }

    @Test
    public void testGetView() {
        assertEquals(mView, mHomePageButtonsCoordinator.getView());
    }

    @Test
    public void testSetVisibility() {
        mHomePageButtonsCoordinator.setVisibility(View.GONE);
        verify(mModel).set(eq(CONTAINER_VISIBILITY), eq(View.GONE));
    }

    @Test
    public void testGetVisibility() {
        when(mModel.get(CONTAINER_VISIBILITY)).thenReturn(View.INVISIBLE);
        assertEquals(View.INVISIBLE, mHomePageButtonsCoordinator.getVisibility());
    }

    @Test
    public void testGetForegroundColor() {
        when(mModel.get(BUTTON_TINT_LIST)).thenReturn(mColorStateList);
        assertEquals(mColorStateList, mHomePageButtonsCoordinator.getForegroundColor());
    }

    @Test
    public void testGetMeasuredWidth() {
        int expectedWidth = 120;
        when(mView.getMeasuredWidth()).thenReturn(expectedWidth);
        assertEquals(expectedWidth, mHomePageButtonsCoordinator.getMeasuredWidth());
    }

    @Test
    public void testSetAccessibilityTraversalBefore() {
        @IdRes int testViewId = R.id.url_bar;
        mHomePageButtonsCoordinator.setAccessibilityTraversalBefore(testViewId);
        verify(mModel).set(eq(ACCESSIBILITY_TRAVERSAL_BEFORE), eq(testViewId));
    }

    @Test
    public void testSetTranslationY() {
        float testTranslationY = 20.0f;
        mHomePageButtonsCoordinator.setTranslationY(testTranslationY);
        verify(mModel).set(eq(TRANSLATION_Y), eq(testTranslationY));
    }

    @Test
    public void testSetClickable() {
        mHomePageButtonsCoordinator.setClickable(true);
        verify(mModel).set(eq(IS_CLICKABLE), eq(true));

        mHomePageButtonsCoordinator.setClickable(false);
        verify(mModel).set(eq(IS_CLICKABLE), eq(false));
    }

    @Test
    public void testSetOnKeyListener() {
        mHomePageButtonsCoordinator.setOnKeyListener(mOnKeyListener);
        verify(mModel).set(eq(ON_KEY_LISTENER), eq(mOnKeyListener));
    }
}
