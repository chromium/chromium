// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.HomeActionProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Unit tests for HomeButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeButtonCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private HomeButton mHomeButton;
    @Mock private Resources mResources;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private ColorStateList mColorStateList;
    @Mock private ClickWithMetaStateCallback mClickCallback;
    @Mock private Callback<Context> mOnMenuClickCallback;

    private boolean mIsHomeButtonMenuDisabled;
    private HomeButtonCoordinator mHomeButtonCoordinator;
    private ActionRegistry mActionRegistry;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        when(mHomeButton.getRootView()).thenReturn(Mockito.mock(View.class));
        when(mHomeButton.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(LayoutInflater.from(ContextUtils.getApplicationContext()));

        mIsHomeButtonMenuDisabled = false;
        mActionRegistry = new ActionRegistry();
        mPropertyModel = new PropertyModel.Builder(HomeActionProperties.ALL_KEYS).build();
        mActionRegistry.register(ActionId.HOME_BUTTON, mPropertyModel);

        mHomeButtonCoordinator =
                new HomeButtonCoordinator(
                        mContext,
                        mHomeButton,
                        mClickCallback,
                        mOnMenuClickCallback,
                        () -> mIsHomeButtonMenuDisabled,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        mActionRegistry);
    }

    @Test
    public void testListMenu() {
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton).showMenu();

        var delegate = mPropertyModel.get(HomeActionProperties.LONG_PRESS_MENU_DELEGATE);
        assertNotNull(delegate);
        delegate.getListMenu();

        assertEquals(1, mHomeButtonCoordinator.getMenuForTesting().size());
    }

    @Test
    public void testListMenuDisabled() {
        mIsHomeButtonMenuDisabled = true;
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton, never()).showMenu();
    }

    @Test
    public void testOnTintChanged() {
        mHomeButtonCoordinator.onTintChanged(
                mColorStateList, mColorStateList, BrandedColorScheme.APP_DEFAULT);
        verify(mHomeButton).setImageTintList(eq(mColorStateList));
    }

    @Test
    public void testModelUpdates() {
        assertEquals(
                mClickCallback, mPropertyModel.get(HomeActionProperties.CLICK_WITH_META_CALLBACK));
        assertNotNull(mPropertyModel.get(HomeActionProperties.LONG_PRESS_MENU_DELEGATE));
    }
}
