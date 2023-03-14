// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.os.Looper;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinatorTablet;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.widget.ToastManager;
import org.chromium.ui.widget.ToastManagerJni;

import java.util.ArrayList;

/**
 * Unit tests for @{@link ToolbarTablet}
 */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public final class ToolbarTabletUnitTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private ToastManager.Natives mToastManagerJni;
    @Mock
    private LocationBarCoordinator mLocationBar;
    @Mock
    private LocationBarCoordinatorTablet mLocationBarTablet;
    @Mock
    private StatusCoordinator mStatusCoordinator;
    @Mock
    private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock
    private View mContainerView;

    private Activity mActivity;
    private ToolbarTablet mToolbarTablet;
    private LinearLayout mToolbarTabletLayout;
    private HomeButton mHomeButton;
    private ImageButton mReloadingButton;
    private ImageButton mBackButton;
    private ImageButton mForwardButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(org.chromium.chrome.tab_ui.R.style.Theme_BrowserUI_DayNight);
        mToolbarTablet = (ToolbarTablet) mActivity.getLayoutInflater().inflate(
                org.chromium.chrome.R.layout.toolbar_tablet, null);
        when(mLocationBar.getTabletCoordinator()).thenReturn(mLocationBarTablet);
        mToolbarTablet.setLocationBarCoordinator(mLocationBar);
        LocationBarLayout locationBarLayout = mToolbarTablet.findViewById(R.id.location_bar);
        locationBarLayout.setStatusCoordinatorForTesting(mStatusCoordinator);
        mToolbarTablet.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        mToolbarTabletLayout =
                (LinearLayout) mToolbarTablet.findViewById(R.id.toolbar_tablet_layout);
        mHomeButton = mToolbarTablet.findViewById(R.id.home_button);
        mBackButton = mToolbarTablet.findViewById(R.id.back_button);
        mForwardButton = mToolbarTablet.findViewById(R.id.forward_button);
        mReloadingButton = mToolbarTablet.findViewById(R.id.refresh_button);
        mJniMocker.mock(ToastManagerJni.TEST_HOOKS, mToastManagerJni);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testButtonPosition() {
        mToolbarTablet.onFinishInflate();
        assertEquals("Home button position is not as expected", mHomeButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals("Back button position is not as expected", mBackButton,
                mToolbarTabletLayout.getChildAt(1));
        assertEquals("Forward button position is not as expected", mForwardButton,
                mToolbarTabletLayout.getChildAt(2));
        assertEquals("Reloading button position is not as expected", mReloadingButton,
                mToolbarTabletLayout.getChildAt(3));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testButtonPositionForTSR() {
        mToolbarTablet.onFinishInflate();
        assertEquals("Back button position is not as expected for Tab Strip Redesign", mBackButton,
                mToolbarTabletLayout.getChildAt(0));
        assertEquals("Forward button position is not as expected for Tab Strip Redesign",
                mForwardButton, mToolbarTabletLayout.getChildAt(1));
        assertEquals("Reloading button position is not as expected for Tab Strip Redesign",
                mReloadingButton, mToolbarTabletLayout.getChildAt(2));
        assertEquals("Home button position is not as expected for Tab Strip Redesign", mHomeButton,
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
            assertEquals("Toolbar button visibility is not as expected", View.VISIBLE,
                    btn.getVisibility());
        }
    }

    @Test
    public void onMeasureSmallWidthWithAnimation_hidesToolbarButtons() {
        for (ImageButton btn : mToolbarTablet.getToolbarButtons()) {
            when(mLocationBar.createHideButtonAnimatorForTablet(btn))
                    .thenReturn(ObjectAnimator.ofFloat(btn, View.ALPHA, 0.f));
        }
        when(mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(new ArrayList<>());

        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();
        // Call
        mToolbarTablet.measure(300, 300);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals(
                    "Toolbar button visibility is not as expected", View.GONE, btn.getVisibility());
        }
    }

    @Test
    public void onMeasureLargeWidthWithAnimation_showsToolbarButtons() {
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
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        ImageButton[] btns = mToolbarTablet.getToolbarButtons();
        for (ImageButton btn : btns) {
            assertEquals("Toolbar button visibility is not as expected", View.VISIBLE,
                    btn.getVisibility());
        }
    }

    @Test
    public void testSetTabSwitcherModeOff_toolbarStillVisible() {
        assertEquals("Initial Toolbar visibility is not as expected", View.VISIBLE,
                mToolbarTablet.getVisibility());
        // Call
        mToolbarTablet.setTabSwitcherMode(false, false, false, mMenuButtonCoordinator);
        assertEquals("Toolbar visibility is not as expected", View.VISIBLE,
                mToolbarTablet.getVisibility());
        verify(mLocationBar).setUrlBarFocusable(true);
    }

    @Test
    public void testSetTabSwitcherModeOn_toolbarStillVisible() {
        assertEquals("Initial Toolbar visibility is not as expected", View.VISIBLE,
                mToolbarTablet.getVisibility());
        // Call
        mToolbarTablet.setTabSwitcherMode(true, false, false, mMenuButtonCoordinator);
        assertEquals("Toolbar visibility is not as expected", View.VISIBLE,
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
        assertEquals("Button description is not as expected",
                mActivity.getResources().getString(R.string.accessibility_btn_stop_loading),
                btn.getContentDescription());
        mToolbarTablet.updateReloadButtonVisibility(false);
        assertEquals("Button drawable level is not as expected", 0, btn.getDrawable().getLevel());
        assertEquals("Button description is not as expected",
                mActivity.getResources().getString(R.string.accessibility_btn_refresh),
                btn.getContentDescription());
        assertTrue("Button should be enabled", btn.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_SCROLL_ABLATION_ANDROID)
    public void testIsReadyForTextureCapture_Ablation() {
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.SCROLL_ABLATION, result.blockReason);
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
        mToolbarTablet.onUrlFocusChange(/*hasFocus*/ true);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS, result.blockReason);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture_InTabSwitcher() {
        mToolbarTablet.setTabSwitcherMode(/*inTabSwitcherMode*/ true, /*showToolbar*/ true,
                /*delayAnimation*/ false, /*menuButtonCoordinator*/ null);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE, result.blockReason);
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

        mToolbarTablet.setTextureCaptureMode(/*textureMode*/ true);

        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertFalse(result.isReady);
            Assert.assertEquals(TopToolbarBlockCaptureReason.SNAPSHOT_SAME, result.blockReason);
        }

        mToolbarTablet.updateBookmarkButton(/*isBookmarked*/ true, /*editingAllowed*/ true);

        {
            CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
            Assert.assertTrue(result.isReady);
            Assert.assertEquals(
                    TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE, result.allowReason);
            Assert.assertEquals(
                    ToolbarSnapshotDifference.BOOKMARK_BUTTON, result.snapshotDifference);
        }
    }

    private void longClickAndVerifyToast(int viewId, int stringId) {
        mToolbarTablet.onLongClick(mToolbarTablet.findViewById(viewId));
        assertTrue("Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(stringId), R.id.toast_text));
    }
}
