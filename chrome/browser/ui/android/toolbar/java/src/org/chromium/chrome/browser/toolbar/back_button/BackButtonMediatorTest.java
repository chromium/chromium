// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.ColorStateList;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class BackButtonMediatorTest {
    private static final int TAB_ID = 0;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock public Runnable mOnBackPressed;
    @Mock public ThemeColorProvider mThemeColorProvider;
    @Mock public Callback<Tab> mShowNavigationPopup;
    @Mock public Profile mProfile;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private PropertyModel mModel;
    private BackButtonMediator mMediator;

    // test properties
    private MockTab mTab;

    @Before
    public void setup() {
        mTab = new MockTab(TAB_ID, mProfile);
        mTabSupplier = new ObservableSupplierImpl<>();
        mModel =
                new PropertyModel.Builder(BackButtonProperties.ALL_KEYS)
                        .with(BackButtonProperties.CLICK_LISTENER, mOnBackPressed)
                        .build();
        mMediator =
                new BackButtonMediator(
                        mModel,
                        mOnBackPressed,
                        mThemeColorProvider,
                        mTabSupplier,
                        mShowNavigationPopup);

        shadowOf(Looper.getMainLooper()).idle();
    }

    private static void verifyEnabled(final PropertyModel model) {
        assertTrue(
                "Button is disabled, but should be enabled.",
                model.get(BackButtonProperties.IS_ENABLED));
        assertTrue(
                "Button is not focusable, but should be focusable.",
                model.get(BackButtonProperties.IS_FOCUSABLE));
    }

    private static void verifyDisabled(final PropertyModel model) {
        assertFalse(
                "Button is enabled, but should be disabled.",
                model.get(BackButtonProperties.IS_ENABLED));
        assertFalse(
                "Button is focusable, but should be not focusable.",
                model.get(BackButtonProperties.IS_FOCUSABLE));
    }

    @Test
    public void testActivityFocusChanged_shouldUpdateFocusTint() {
        var tint = mock(ColorStateList.class);
        var focusTint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, focusTint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Activity focus tint list should be used, but was another tint",
                mModel.get(BackButtonProperties.TINT_COLOR_LIST),
                focusTint);
    }

    @Test
    public void testThemeChangedToAppDefault_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToLightTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToDarkTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToIncognito_shouldSetIncognitoRipple() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.INCOGNITO);

        assertEquals(
                "Background ripple effect should be incognito",
                mModel.get(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE),
                org.chromium.chrome.browser.toolbar.R.drawable.default_icon_background_baseline);
    }

    @Test
    public void testClick_shouldForwardCallToParent() {
        mModel.get(BackButtonProperties.CLICK_LISTENER).run();
        verify(mOnBackPressed).run();
    }

    @Test
    public void testLongClickNoTab_shouldNotForwardCallToParent() {
        mModel.get(BackButtonProperties.LONG_CLICK_LISTENER).run();
        verifyNoInteractions(mShowNavigationPopup);
    }

    @Test
    public void testLongClick_shouldForwardCallToParent() {
        mTabSupplier.set(mTab);

        mModel.get(BackButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowNavigationPopup).onResult(mTab);
    }

    @Test
    public void testNewTabWithNoHistory_shouldKeepButtonDisabled() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        verifyDisabled(mModel);
    }

    @Test
    public void testNewTabWithHistory_shouldEnableButton() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        verifyDisabled(mModel);
    }

    @Test
    public void testTabChangesToNonInteractive_shouldKeepEnabledButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);
        verifyEnabled(mModel);

        mTabSupplier.set(null);
        verifyEnabled(mModel);
    }

    @Test
    public void testLoadingStartedCanGoBack_shouldEnableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onLoadStarted(mTab, /* toDifferentDocument= */ false);
        verifyEnabled(mModel);
    }

    @Test
    public void testLoadingStartedCantGoBack_shouldDisableButton() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onLoadStarted(mTab, /* toDifferentDocument= */ false);
        verifyDisabled(mModel);
    }

    @Test
    public void testLoadingStoppedCanGoBack_shouldEnableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onLoadStopped(mTab, /* toDifferentDocument= */ false);
        verifyEnabled(mModel);
    }

    @Test
    public void testLoadingStoppedCantGoBack_shouldDisableButton() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onLoadStopped(mTab, /* toDifferentDocument= */ false);
        verifyDisabled(mModel);
    }

    @Test
    public void testHistoryDeleted_shouldDisableButton() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onNavigationEntriesDeleted(mTab);
        verifyDisabled(mModel);
    }

    @Test
    public void testUrlUpdatedCanGoBack_shouldEnableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onUrlUpdated(mTab);
        verifyEnabled(mModel);
    }

    @Test
    public void testUrlUpdatedCantGoBack_shouldDisableButton() {
        mTab.setCanGoBack(false);
        mTabSupplier.set(mTab);

        mMediator.getTabObserver().onUrlUpdated(mTab);
        verifyDisabled(mModel);
    }

    @Test
    public void testEnterExitTabSwitcherMode_shouldDisableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);

        mMediator.setTabSwitcherMode(true);
        verifyDisabled(mModel);
    }

    @Test
    public void testExitedTabSwitcherMode_shouldEnableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);
        mMediator.setTabSwitcherMode(true);

        mMediator.setTabSwitcherMode(false);
        verifyEnabled(mModel);
    }
}
