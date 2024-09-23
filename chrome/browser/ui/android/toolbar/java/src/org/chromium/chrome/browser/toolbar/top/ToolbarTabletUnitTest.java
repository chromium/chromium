// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinatorTablet;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.widget.ToastManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for @{@link ToolbarTablet} */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public final class ToolbarTabletUnitTest {
    private static final int TAB_COUNT = 1;
    @Mock private LocationBarCoordinator mLocationBar;
    @Mock private LocationBarCoordinatorTablet mLocationBarTablet;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    @Mock private StatusCoordinator mStatusCoordinator;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private TabStripTransitionCoordinator mTabStripTransitionCoordinator;
    @Mock private ToolbarColorObserver mToolbarColorObserver;
    @Mock private ToolbarDataProvider mToolbarDataProvider;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private ToolbarTabController mToolbarTabController;
    private Activity mActivity;
    private ToolbarTablet mToolbarTablet;
    private LinearLayout mToolbarTabletLayout;
    private ImageButton mHomeButton;
    private ImageButton mReloadingButton;
    private ImageButton mBackButton;
    private ImageButton mForwardButton;
    private ImageButton mMenuButton;
    private ToggleTabStackButton mTabSwitcherButton;
    private ImageButton mBookmarkButton;
    private ImageButton mSaveOfflineButton;
    private View mLocationBarButton;
    private ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(TAB_COUNT);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ToolbarTablet realView =
                (ToolbarTablet)
                        mActivity.getLayoutInflater().inflate(R.layout.toolbar_tablet, null);
        mToolbarTablet = Mockito.spy(realView);
        when(mLocationBar.getTabletCoordinator()).thenReturn(mLocationBarTablet);
        mToolbarTablet.setLocationBarCoordinator(mLocationBar);
        LocationBarLayout locationBarLayout = mToolbarTablet.findViewById(R.id.location_bar);
        locationBarLayout.setStatusCoordinatorForTesting(mStatusCoordinator);
        mToolbarTablet.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        mToolbarTablet.setTabSwitcherButtonCoordinatorForTesting(mTabSwitcherButtonCoordinator);
        mToolbarTablet.setTabStripTransitionCoordinator(mTabStripTransitionCoordinator);
        mToolbarTablet.setToolbarColorObserver(mToolbarColorObserver);
        mToolbarTabletLayout =
                (LinearLayout) mToolbarTablet.findViewById(R.id.toolbar_tablet_layout);
        mHomeButton = mToolbarTablet.findViewById(R.id.home_button);
        mBackButton = mToolbarTablet.findViewById(R.id.back_button);
        mForwardButton = mToolbarTablet.findViewById(R.id.forward_button);
        mReloadingButton = mToolbarTablet.findViewById(R.id.refresh_button);
        mMenuButton = mToolbarTablet.findViewById(R.id.menu_button);
        mTabSwitcherButton = mToolbarTablet.findViewById(R.id.tab_switcher_button);
        when(mTabSwitcherButtonCoordinator.getContainerView()).thenReturn(mTabSwitcherButton);
        mLocationBarButton = mToolbarTablet.findViewById(R.id.location_bar_status_icon);
        mBookmarkButton = mToolbarTablet.findViewById(R.id.bookmark_button);
        mSaveOfflineButton = mToolbarTablet.findViewById(R.id.save_offline_button);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
    }

    @Test
    public void testButtonPosition() {
        mToolbarTablet.onFinishInflate();
        assertEquals(
                "Home button position is not as expected",
                mHomeButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals(
                "Back button position is not as expected",
                mBackButton,
                mToolbarTabletLayout.getChildAt(1));
        assertEquals(
                "Forward button position is not as expected",
                mForwardButton,
                mToolbarTabletLayout.getChildAt(2));
        assertEquals(
                "Reloading button position is not as expected",
                mReloadingButton,
                mToolbarTabletLayout.getChildAt(3));
    }

    @Test
    public void testReloadButton() {
        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                mToolbarTabController,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                () -> false,
                null,
                null,
                null);
        mToolbarTablet.onNativeLibraryReady();

        mToolbarTablet.onClick(mReloadingButton);

        verify(mToolbarTabController).stopOrReloadCurrentTab(false);
    }

    @Test
    public void testReloadButton_shiftDown() {
        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                mToolbarTabController,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                () -> false,
                null,
                null,
                null);
        mToolbarTablet.onNativeLibraryReady();

        MotionEvent shiftClick =
                MotionEvent.obtain(
                        0, 0, MotionEvent.ACTION_BUTTON_PRESS, 0, 0, KeyEvent.META_SHIFT_ON);
        mToolbarTablet.getReloadButtonTouchListenerForTest().onTouch(mReloadingButton, shiftClick);
        mToolbarTablet.onClick(mReloadingButton);

        verify(mToolbarTabController).stopOrReloadCurrentTab(true);
    }

    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @Test
    public void testButtonPositionIncognito() {
        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                () -> false,
                null,
                null,
                null);
        when(mToolbarDataProvider.getNewTabPageDelegate()).thenReturn(mNewTabPageDelegate);
        when(mToolbarDataProvider.isIncognitoBranded()).thenReturn(true);
        mToolbarTablet.onTabOrModelChanged();

        assertEquals(
                "Home button position is not as expected",
                mHomeButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals(
                "Back button position is not as expected",
                mBackButton,
                mToolbarTabletLayout.getChildAt(1));
        assertEquals(
                "Forward button position is not as expected",
                mForwardButton,
                mToolbarTabletLayout.getChildAt(2));
        assertEquals(
                "Reloading button position is not as expected",
                mReloadingButton,
                mToolbarTabletLayout.getChildAt(3));
        View incognitoIndicator = mToolbarTabletLayout.findViewById(R.id.incognito_indicator);
        assertNotNull("Incognito indicator is not inflated", incognitoIndicator);
        assertEquals(
                "Incognito indicator visibility is not as expected.",
                View.VISIBLE,
                incognitoIndicator.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TABLET_TOOLBAR_REORDERING})
    public void testButtonPosition_TSR() {
        mToolbarTablet.onFinishInflate();
        assertEquals(
                "Back button position is not as expected for Tab Strip Redesign",
                mBackButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals(
                "Forward button position is not as expected for Tab Strip Redesign",
                mForwardButton,
                mToolbarTabletLayout.getChildAt(1));
        assertEquals(
                "Reloading button position is not as expected for Tab Strip Redesign",
                mReloadingButton,
                mToolbarTabletLayout.getChildAt(2));
        assertEquals(
                "Home button position is not as expected for Tab Strip Redesign",
                mHomeButton,
                mToolbarTabletLayout.getChildAt(3));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TABLET_TOOLBAR_REORDERING)
    public void testButtonPosition_ShutoffToolbarReordering() {
        mToolbarTablet.onFinishInflate();

        assertEquals(
                "Home button position is not as expected for TSR disable Toolbar reordering",
                mHomeButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals(
                "Back button position is not as expected for TSR disable Toolbar reordering",
                mBackButton,
                mToolbarTabletLayout.getChildAt(1));
        assertEquals(
                "Forward button position is not as expected for TSR disable Toolbar reordering",
                mForwardButton,
                mToolbarTabletLayout.getChildAt(2));
        assertEquals(
                "Reloading button position is not as expected for TSR disable Toolbar reordering",
                mReloadingButton,
                mToolbarTabletLayout.getChildAt(3));
    }

    @Test
    public void onMeasureShortWidth_hidesToolbarButtons() {
        mToolbarTablet.measure(300, 300);

        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals(
                    "Toolbar button visibility is not as expected", View.GONE, btn.getVisibility());
        }
    }

    @Test
    public void onMeasureLargeWidth_showsToolbarButtons() {
        mToolbarTablet.measure(700, 300);

        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals(
                    "Toolbar button visibility is not as expected",
                    View.VISIBLE,
                    btn.getVisibility());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void onMeasureIncognito_flipIncognitoVisibility() {
        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                () -> false,
                null,
                null,
                null);
        when(mToolbarDataProvider.getNewTabPageDelegate()).thenReturn(mNewTabPageDelegate);
        when(mToolbarDataProvider.isIncognitoBranded()).thenReturn(true);
        mToolbarTablet.onTabOrModelChanged();
        View incognitoIndicator = mToolbarTablet.findViewById(R.id.incognito_indicator);
        assertNotNull(incognitoIndicator);
        // Measure with wide width - indicator visible
        mToolbarTablet.measure(700, 300);
        assertEquals(
                "Incognito indicator visibility is not as expected.",
                View.VISIBLE,
                incognitoIndicator.getVisibility());

        // Measure with smaller width - indicator invisible
        mToolbarTablet.measure(300, 300);
        assertEquals(
                "Incognito indicator visibility is not as expected.",
                View.GONE,
                incognitoIndicator.getVisibility());
    }

    @Test
    public void onMeasureSmallWidthWithAnimation_hidesToolbarButtons() {
        doReturn(true).when(mToolbarTablet).isShown();
        for (ImageButton btn : mToolbarTablet.getToolbarButtons()) {
            when(mLocationBar.createHideButtonAnimatorForTablet(btn))
                    .thenReturn(ObjectAnimator.ofFloat(btn, View.ALPHA, 0.f));
        }
        when(mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(new ArrayList<>());

        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();
        // Call
        mToolbarTablet.measure(300, 300);
        verify(mTabStripTransitionCoordinator).requestDeferTabStripTransitionToken();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals(
                    "Toolbar button visibility is not as expected", View.GONE, btn.getVisibility());
        }
        verify(mTabStripTransitionCoordinator, atLeastOnce()).releaseTabStripToken(anyInt());
    }

    @Test
    public void onMeasureLargeWidthWithAnimation_showsToolbarButtons() {
        doReturn(true).when(mToolbarTablet).isShown();
        mToolbarTablet.setToolbarButtonsVisibleForTesting(false);
        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();
        for (ImageButton btn : mToolbarTablet.getToolbarButtons()) {
            when(mLocationBar.createShowButtonAnimatorForTablet(btn))
                    .thenReturn(ObjectAnimator.ofFloat(btn, View.ALPHA, 1.f));
        }
        when(mLocationBar.getShowButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(new ArrayList<>());
        // Call
        mToolbarTablet.measure(700, 300);
        verify(mTabStripTransitionCoordinator).requestDeferTabStripTransitionToken();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals(
                    "Toolbar button visibility is not as expected",
                    View.VISIBLE,
                    btn.getVisibility());
        }
        verify(mTabStripTransitionCoordinator, atLeastOnce()).releaseTabStripToken(anyInt());
    }

    @Test
    public void testSetTabSwitcherModeOff_toolbarStillVisible() {
        assertEquals(
                "Initial Toolbar visibility is not as expected",
                View.VISIBLE,
                mToolbarTablet.getVisibility());
        // Call
        mToolbarTablet.setTabSwitcherMode(false);
        assertEquals(
                "Toolbar visibility is not as expected",
                View.VISIBLE,
                mToolbarTablet.getVisibility());
        verify(mLocationBar).setUrlBarFocusable(true);
    }

    @Test
    public void testSetTabSwitcherModeOn_toolbarStillVisible() {
        assertEquals(
                "Initial Toolbar visibility is not as expected",
                View.VISIBLE,
                mToolbarTablet.getVisibility());
        // Call
        mToolbarTablet.setTabSwitcherMode(true);
        assertEquals(
                "Toolbar visibility is not as expected",
                View.VISIBLE,
                mToolbarTablet.getVisibility());
        verify(mLocationBar).setUrlBarFocusable(false);
    }

    @Test
    public void testOnLongClick() {
        longClickAndVerifyToast(R.id.refresh_button, R.string.refresh);
        longClickAndVerifyToast(R.id.bookmark_button, R.string.menu_bookmark);
        longClickAndVerifyToast(R.id.save_offline_button, R.string.menu_download);
    }

    @Test
    public void testUpdateBackButtonVisibility() {
        ImageButton btn = mToolbarTablet.findViewById(R.id.back_button);
        mToolbarTablet.updateBackButtonVisibility(true);
        assertTrue("Button should be enabled", btn.isEnabled());
        assertTrue("Button should be focused", btn.isFocusable());
        mToolbarTablet.updateBackButtonVisibility(false);
        assertFalse("Button should not be enabled", btn.isEnabled());
        assertFalse("Button should not be focused", btn.isFocusable());
    }

    @Test
    public void testUpdateForwardButtonVisibility() {
        ImageButton btn = mToolbarTablet.findViewById(R.id.forward_button);
        mToolbarTablet.updateForwardButtonVisibility(true);
        assertTrue("Button should be enabled", btn.isEnabled());
        assertTrue("Button should be focused", btn.isFocusable());
        mToolbarTablet.updateForwardButtonVisibility(false);
        assertFalse("Button should not be enabled", btn.isEnabled());
        assertFalse("Button should not be focused", btn.isFocusable());
    }

    @Test
    public void testUpdateReloadButtonVisibility() {
        ImageButton btn = mToolbarTablet.findViewById(R.id.refresh_button);
        mToolbarTablet.updateReloadButtonVisibility(true);
        assertTrue("Button should be enabled", btn.isEnabled());
        assertEquals("Button drawable level is not as expected", 1, btn.getDrawable().getLevel());
        assertEquals(
                "Button description is not as expected",
                mActivity.getResources().getString(R.string.accessibility_btn_stop_loading),
                btn.getContentDescription());
        mToolbarTablet.updateReloadButtonVisibility(false);
        assertEquals("Button drawable level is not as expected", 0, btn.getDrawable().getLevel());
        assertEquals(
                "Button description is not as expected",
                mActivity.getResources().getString(R.string.accessibility_btn_refresh),
                btn.getContentDescription());
        assertTrue("Button should be enabled", btn.isEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_NoSuppression() {
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertTrue(result.isReady);
        Assert.assertEquals(TopToolbarAllowCaptureReason.UNKNOWN, result.allowReason);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_HasFocus() {
        mToolbarTablet.onUrlFocusChange(/* hasFocus= */ true);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS, result.blockReason);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testHoverTooltipText() {
        mTabSwitcherButton.setTabCountSupplier(mTabCountSupplier, () -> true);
        mTabSwitcherButton.onDrawableStateChanged();

        // verify tooltip texts for tablet toolbar button are set.
        Assert.assertEquals(
                "Tooltip text for Home button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_toolbar_btn_home),
                mHomeButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Reload button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_btn_refresh),
                mReloadingButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Forward button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_menu_forward),
                mForwardButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Back button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_toolbar_btn_back),
                mBackButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Tab Switcher button is not as expected",
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.accessibility_toolbar_btn_tabswitcher_toggle_default,
                                TAB_COUNT,
                                TAB_COUNT),
                mTabSwitcherButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Bookmark button is not as expected",
                mActivity.getResources().getString(R.string.accessibility_menu_bookmark),
                mBookmarkButton.getTooltipText());
        Assert.assertEquals(
                "Tooltip text for Save Offline button is not as expected",
                mActivity.getResources().getString(R.string.download_page),
                mSaveOfflineButton.getTooltipText());
    }

    @Test
    public void testOptionalButtonTooltipText() {
        Drawable iconDrawable = AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.actionbar_share);
        ButtonDataImpl buttonData = new ButtonDataImpl();

        // Verify reader mode tooltip text is null.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.READER_MODE,
                        0,
                        0,
                        false);
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);
        Assert.assertEquals(
                null, mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether share button tooltip Text is set correctly.
        buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                        0,
                        R.string.adaptive_toolbar_button_preference_share,
                        true);
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_share),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether voice search button tooltip Text is set correctly.
        buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.VOICE,
                        0,
                        R.string.adaptive_toolbar_button_preference_voice_search,
                        true);
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_voice_search),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether new tab button tooltip Text is set correctly.
        buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.NEW_TAB,
                        0,
                        R.string.new_tab_title,
                        true);
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.new_tab_title),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_InTabSwitcher() {
        mToolbarTablet.setTabSwitcherMode(/* inTabSwitcherMode= */ true);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE, result.blockReason);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_ButtonShowAnimationInProgress() {
        mToolbarTablet.setToolbarButtonsVisibleForTesting(false);
        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();

        // Set a test-only animator so the animation can have an in-between state.
        ValueAnimator animator = ValueAnimator.ofFloat(0.f, 1.f);
        when(mLocationBar.getShowButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(List.of(animator));

        // Run animation.
        mToolbarTablet.measure(700, 300);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(
                TopToolbarBlockCaptureReason.TABLET_BUTTON_ANIMATION_IN_PROGRESS,
                result.blockReason);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertTrue(result.isReady);
        Assert.assertEquals(TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, result.allowReason);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_ButtonHideAnimationInProgress() {
        mToolbarTablet.setToolbarButtonsVisibleForTesting(true);
        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();

        // Set a test-only animator so the animation can have an in-between state.
        ValueAnimator animator = ValueAnimator.ofFloat(0.f, 1.f);
        when(mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(List.of(animator));

        // Run animation.
        mToolbarTablet.measure(300, 300);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(
                TopToolbarBlockCaptureReason.TABLET_BUTTON_ANIMATION_IN_PROGRESS,
                result.blockReason);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertTrue(result.isReady);
        Assert.assertEquals(TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, result.allowReason);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_Snapshot() {
        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertTrue(result.isReady);
            Assert.assertEquals(
                    TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, result.allowReason);
            Assert.assertEquals(ToolbarSnapshotDifference.NULL, result.snapshotDifference);
        }

        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertTrue(result.isReady);
        }

        mToolbarTablet.setTextureCaptureMode(/* textureMode= */ true);

        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertFalse(result.isReady);
            Assert.assertEquals(TopToolbarBlockCaptureReason.SNAPSHOT_SAME, result.blockReason);
        }

        mToolbarTablet.updateBookmarkButton(/* isBookmarked= */ true, /* editingAllowed= */ true);

        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertTrue(result.isReady);
            Assert.assertEquals(
                    TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, result.allowReason);
            Assert.assertEquals(
                    ToolbarSnapshotDifference.BOOKMARK_BUTTON, result.snapshotDifference);
        }
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testThemeColorChange() {
        int color = Color.BLACK;
        mToolbarTablet.onThemeColorChanged(color, false);
        // Verify that ToolbarColorObserver is notified of the color change.
        verify(mToolbarColorObserver).onToolbarColorChanged(color);
    }

    @Test
    public void testOnTintChanged_UnfocusedActivityTint() {
        var tint =
                ThemeUtils.getThemedToolbarIconTint(
                        mToolbarTablet.getContext(), BrandedColorScheme.APP_DEFAULT);
        var unfocusedTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mToolbarTablet.getContext(),
                        BrandedColorScheme.APP_DEFAULT,
                        /* isActivityFocused= */ false);
        // Setup.
        ButtonDataImpl buttonData = new ButtonDataImpl();
        var buttonSpec =
                new ButtonSpec(
                        AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon),
                        (OnClickListener) v -> {},
                        (OnLongClickListener) v -> false,
                        "",
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.NEW_TAB,
                        0,
                        R.string.adaptive_toolbar_button_preference_new_tab,
                        true);
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);

        // Verify the toolbar icon tints, assuming that the activity is initially focused.
        verifyToolbarIconTints(tint, tint);

        // Simulate a tint change triggered when the activity loses focus.
        mToolbarTablet.onTintChanged(tint, unfocusedTint, BrandedColorScheme.APP_DEFAULT);

        // Verify the icon tints for the unfocused activity.
        verifyToolbarIconTints(tint, unfocusedTint);
        Assert.assertEquals(
                "Tab switcher button tint is incorrect.",
                unfocusedTint.getDefaultColor(),
                mTabSwitcherButton.getImageTintList().getDefaultColor());
    }

    private void verifyToolbarIconTints(ColorStateList tint, ColorStateList activityFocusTint) {
        Assert.assertEquals(
                "Home button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mHomeButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Back button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mBackButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Forward button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mForwardButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Reload button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mReloadingButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Save offline button tint is incorrect.",
                tint.getDefaultColor(),
                mSaveOfflineButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Bookmark button tint is incorrect.",
                tint.getDefaultColor(),
                mBookmarkButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Optional button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                ((ImageButton) mToolbarTablet.getOptionalButtonViewForTesting())
                        .getImageTintList()
                        .getDefaultColor());
    }

    private void longClickAndVerifyToast(int viewId, int stringId) {
        mToolbarTablet.onLongClick(mToolbarTablet.findViewById(viewId));
        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(stringId), R.id.toast_text));
        ToastManager.resetForTesting();
    }
}
