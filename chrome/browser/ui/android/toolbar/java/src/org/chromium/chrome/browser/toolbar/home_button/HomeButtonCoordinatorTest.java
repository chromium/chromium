// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for HomeButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeButtonCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private HomeButton mHomeButton;
    @Mock private android.content.res.Resources mResources;
    @Mock private View.OnKeyListener mOnKeyListener;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private ColorStateList mColorStateList;

    private boolean mIsHomeButtonMenuDisabled;
    private HomeButtonCoordinator mHomeButtonCoordinator;

    @Before
    public void setUp() {
        when(mHomeButton.getRootView()).thenReturn(Mockito.mock(View.class));
        when(mHomeButton.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(LayoutInflater.from(ContextUtils.getApplicationContext()));

        mIsHomeButtonMenuDisabled = false;
        mHomeButtonCoordinator =
                new HomeButtonCoordinator(
                        mContext,
                        mHomeButton,
                        (view) -> {},
                        (context) -> {},
                        () -> mIsHomeButtonMenuDisabled,
                        mThemeColorProvider,
                        mIncognitoStateProvider);
    }

    @Test
    public void testListMenu() {
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton).showMenu();
        assertEquals(1, mHomeButtonCoordinator.getMenuForTesting().size());
    }

    @Test
    public void testListMenuDisabled() {
        mIsHomeButtonMenuDisabled = true;
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton, never()).showMenu();
    }

    @Test
    public void testGetView() {
        assertEquals(mHomeButton, mHomeButtonCoordinator.getView());
    }

    @Test
    public void testSetVisibility() {
        mHomeButtonCoordinator.setVisibility(View.GONE);
        verify(mHomeButton).setVisibility(View.GONE);

        mHomeButtonCoordinator.setVisibility(View.VISIBLE);
        verify(mHomeButton).setVisibility(View.VISIBLE);
    }

    @Test
    public void testGetVisibility() {
        when(mHomeButton.getVisibility()).thenReturn(View.INVISIBLE);
        assertEquals(View.INVISIBLE, mHomeButtonCoordinator.getVisibility());
    }

    @Test
    public void testOnTintChanged() {
        mHomeButtonCoordinator.onTintChanged(
                mColorStateList, mColorStateList, BrandedColorScheme.APP_DEFAULT);
        verify(mHomeButton).setImageTintList(eq(mColorStateList));
    }

    @Test
    public void testSetBackgroundResource() {
        @DrawableRes int testResId = R.drawable.default_icon_background_baseline;
        mHomeButtonCoordinator.setBackgroundResource(testResId);
        verify(mHomeButton).setBackgroundResource(testResId);
    }

    @Test
    public void testGetMeasuredWidth() {
        int expectedWidth = 100;
        when(mHomeButton.getMeasuredWidth()).thenReturn(expectedWidth);
        assertEquals(expectedWidth, mHomeButtonCoordinator.getMeasuredWidth());
    }

    @Test
    public void testUpdateState() {
        mHomeButtonCoordinator.updateState(
                VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ false);
        verify(mHomeButton).setVisibility(View.VISIBLE);

        mHomeButtonCoordinator.updateState(
                VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ true,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ true);
        verify(mHomeButton).setVisibility(View.INVISIBLE);

        mHomeButtonCoordinator.updateState(
                VisualState.NEW_TAB_NORMAL,
                /* isHomeButtonEnabled= */ false,
                /* isHomepageNonNtp= */ false,
                /* urlHasFocus= */ false);
        verify(mHomeButton).setVisibility(View.GONE);
    }

    @Test
    public void testSetAccessibilityTraversalBefore() {
        int testViewId = R.id.url_bar;
        mHomeButtonCoordinator.setAccessibilityTraversalBefore(testViewId);
        verify(mHomeButton).setAccessibilityTraversalBefore(testViewId);
    }

    @Test
    public void testSetTranslationY() {
        float testTranslationY = 50.0f;
        mHomeButtonCoordinator.setTranslationY(testTranslationY);
        verify(mHomeButton).setTranslationY(testTranslationY);
    }

    @Test
    public void testSetClickable() {
        mHomeButtonCoordinator.setClickable(true);
        verify(mHomeButton).setClickable(true);

        mHomeButtonCoordinator.setClickable(false);
        verify(mHomeButton).setClickable(false);
    }

    @Test
    public void testSetOnKeyListener() {
        mHomeButtonCoordinator.setOnKeyListener(mOnKeyListener);
        verify(mHomeButton).setOnKeyListener(mOnKeyListener);

        mHomeButtonCoordinator.setOnKeyListener(null);
        verify(mHomeButton).setOnKeyListener(null);
    }
}
