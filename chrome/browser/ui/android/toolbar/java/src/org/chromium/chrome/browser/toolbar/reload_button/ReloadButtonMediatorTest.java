// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.MotionEvent;

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
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ReloadButtonMediatorTest {
    private static final String STOP_LOADING_DESCRIPTION = "Stop loading";
    private static final String STOP_TOAST_MSG = "Stop";
    private static final String RELOAD_DESCRIPTION = "Reload";
    private static final String RELOAD_TOAST_MSG = "Reload";
    private static final int RELOAD_LEVEL = 0;
    private static final int STOP_LEVEL = 1;
    private static final int TAB_ID = 0;
    private static final int ANOTHER_TAB_ID = 1;
    private static final int NTP_ID = 2;

    @Rule public MockitoRule mockitoTestRule = MockitoJUnit.rule();

    @Mock public ReloadButtonCoordinator.Delegate mDelegate;
    @Mock public Callback<String> mShowToastCallback;
    @Mock public ThemeColorProvider mThemeColorProvider;

    @Mock public Resources mResources;
    @Mock public Context mContext;
    @Mock public Profile mProfile;
    private MockTab mTab;
    private MockTab mNtpTab;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<Boolean> mNtpLoadingSupplier;
    private ObservableSupplierImpl<Boolean> mEnabledSupplier;
    private PropertyModel mModel;
    private ReloadButtonMediator mMediator;

    @Before
    public void setup() {
        when(mResources.getString(R.string.accessibility_btn_stop_loading))
                .thenReturn(STOP_LOADING_DESCRIPTION);
        when(mResources.getString(R.string.accessibility_btn_refresh))
                .thenReturn(RELOAD_DESCRIPTION);
        when(mResources.getInteger(R.integer.reload_button_level_stop)).thenReturn(STOP_LEVEL);
        when(mResources.getInteger(R.integer.reload_button_level_reload)).thenReturn(RELOAD_LEVEL);
        when(mResources.getString(R.string.refresh)).thenReturn(RELOAD_TOAST_MSG);
        when(mResources.getString(R.string.menu_stop_refresh)).thenReturn(STOP_TOAST_MSG);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);

        mTab = new MockTab(TAB_ID, mProfile);
        mNtpTab = new MockTab(NTP_ID, mProfile);
        mNtpTab.setIsNativePage(true);

        mTabSupplier = new ObservableSupplierImpl<>(mTab);
        mNtpLoadingSupplier = new ObservableSupplierImpl<>();
        mEnabledSupplier = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS).build();
        mMediator =
                new ReloadButtonMediator(
                        mModel,
                        mDelegate,
                        mThemeColorProvider,
                        mTabSupplier,
                        mNtpLoadingSupplier,
                        mEnabledSupplier,
                        mShowToastCallback,
                        mResources,
                        mContext,
                        /* isWebApp= */ false);

        // supplier will try to notify observers initially, need to wait for updates.
        shadowOf(Looper.getMainLooper()).idle();
    }

    private static void verifyStopLoadingState(PropertyModel model) {
        assertEquals(
                "Reload icon should be stop",
                STOP_LEVEL,
                model.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be stop",
                STOP_LOADING_DESCRIPTION,
                model.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    private static void verifyStartLoadingState(PropertyModel model) {
        assertEquals(
                "Reload icon should be reload",
                RELOAD_LEVEL,
                model.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be reload",
                RELOAD_DESCRIPTION,
                model.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testClicksWithoutShift_reloadTabWithCache() {
        final MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(false);
    }

    @Test
    public void testClicksWithShift_reloadTabIgnoringCache() {
        final MotionEvent event =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, KeyEvent.META_SHIFT_ON);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(true);
    }

    @Test
    public void testLongClickReloading_showStopToast() {
        mTab.onLoadStarted(/* toDifferentDocument= */ true);

        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(STOP_TOAST_MSG);
    }

    @Test
    public void testLongClickIdle_showReloadToast() {
        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(RELOAD_TOAST_MSG);
    }

    @Test
    public void testChangeToVisible_showButton() {
        mMediator.setVisibility(true);
        assertTrue("Reload button is not visible", mModel.get(ReloadButtonProperties.IS_VISIBLE));
    }

    @Test
    public void testChangeToHidden_hideButton() {
        mMediator.setVisibility(false);
        assertFalse("Reload button is visible", mModel.get(ReloadButtonProperties.IS_VISIBLE));
    }

    @Test
    public void testPrepareFadeInAnimation_shouldSetAlpha0() {
        mModel.set(ReloadButtonProperties.ALPHA, 1);

        mMediator.getFadeAnimator(true);
        assertEquals(
                "Alpha should be set to 0", mModel.get(ReloadButtonProperties.ALPHA), 0f, 0.01f);
    }

    @Test
    public void testPrepareFadeOutAnimation_shouldSetAlpha1() {
        mModel.set(ReloadButtonProperties.ALPHA, 0f);

        mMediator.getFadeAnimator(false);
        assertEquals(
                "Alpha should be set to 1", mModel.get(ReloadButtonProperties.ALPHA), 1f, 0.01f);
    }

    @Test
    public void testActivityFocusChanged_shouldUpdateFocusTint() {
        var tint = mock(ColorStateList.class);
        var focusTint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, focusTint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Activity focus tint list should be used, but was another tint",
                mModel.get(ReloadButtonProperties.TINT_LIST),
                focusTint);
    }

    @Test
    public void testThemeChangedToAppDefault_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                "Background ripple effect should be default",
                mMediator.getBackgroundResForTesting(),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToLightTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mMediator.getBackgroundResForTesting(),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToDarkTheme_shouldSetDefaultRippleBackground() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.DARK_BRANDED_THEME);

        assertEquals(
                "Background ripple effect should be default",
                mMediator.getBackgroundResForTesting(),
                R.drawable.default_icon_background);
    }

    @Test
    public void testThemeChangedToIncognito_shouldSetIncognitoRipple() {
        var tint = mock(ColorStateList.class);
        mMediator.onTintChanged(tint, tint, BrandedColorScheme.INCOGNITO);

        assertEquals(
                "Background ripple effect should be incognito",
                mMediator.getBackgroundResForTesting(),
                R.drawable.default_icon_background_baseline);
    }

    @Test
    public void testInitialTabSet_shouldSetStoppedState() {
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testTabStartedLoading_shouldSetReloadingState() {
        mTab.onLoadStarted(/* toDifferentDocument= */ true);
        verifyStopLoadingState(mModel);
    }

    @Test
    public void testTabStartedLoadingToSameDocument_shouldKeepStoppedState() {
        mTab.onLoadStarted(/* toDifferentDocument= */ false);
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testTabStartStopLoading_shouldSetLoadingThenStopped() {
        mTab.onLoadStarted(/* toDifferentDocument= */ true);
        verifyStopLoadingState(mModel);

        mTab.onLoadStopped();
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testTabCrashDuringLoading_shouldSetStoppedState() {
        mTab.onLoadStarted(/* toDifferentDocument= */ true);
        verifyStopLoadingState(mModel);

        mTab.handleTabCrash();
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testNewStoppedTabChangesWhileLoadingCurrent_shouldSetStoppedState() {
        mTab.onLoadStarted(/* toDifferentDocument= */ true);
        verifyStopLoadingState(mModel);

        var newTab = new MockTab(ANOTHER_TAB_ID, mProfile);
        mTabSupplier.set(newTab);
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testPageTabWithNtpReloading_shouldNotChangeFromStop() {
        mNtpLoadingSupplier.set(true);
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testSwapToNtpAndStartLoading_shouldBeInStopLoadingState() {
        mTabSupplier.set(mNtpTab);
        mNtpLoadingSupplier.set(true);
        verifyStopLoadingState(mModel);
    }

    @Test
    public void testNtpStopLoading_shouldBeInStartLoadingState() {
        mTabSupplier.set(mNtpTab);
        mNtpLoadingSupplier.set(true);
        verifyStopLoadingState(mModel);

        mNtpLoadingSupplier.set(false);
        verifyStartLoadingState(mModel);
    }

    @Test
    public void testDisabledControl_shouldDisableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);
        mEnabledSupplier.set(false);

        assertFalse(
                "Button is enabled, but should be disabled.",
                mModel.get(ReloadButtonProperties.IS_ENABLED));
    }

    @Test
    public void testEnabledControl_shouldEnableButton() {
        mTab.setCanGoBack(true);
        mTabSupplier.set(mTab);
        mEnabledSupplier.set(false);
        mEnabledSupplier.set(true);

        assertTrue(
                "Button is disabled, but should be enabled.",
                mModel.get(ReloadButtonProperties.IS_ENABLED));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        assertNull(
                "Touch listener should be set to null",
                mModel.get(ReloadButtonProperties.TOUCH_LISTENER));
        assertNull(
                "Click listener should be set to null",
                mModel.get(ReloadButtonProperties.CLICK_LISTENER));
    }

    @Test
    public void testSetBackgroundInsets() {
        final var insets = androidx.core.graphics.Insets.of(1, 2, 3, 4);
        mMediator.setBackgroundInsets(insets);
        assertEquals(
                "Padding should be equal to insets.",
                insets,
                mModel.get(ReloadButtonProperties.PADDING));
    }
}
