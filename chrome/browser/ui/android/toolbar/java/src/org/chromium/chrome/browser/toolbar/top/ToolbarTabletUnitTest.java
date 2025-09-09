// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar.top;

import static android.view.View.MeasureSpec.EXACTLY;
import static android.view.View.MeasureSpec.UNSPECIFIED;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.geq;
import static org.mockito.AdditionalMatchers.lt;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.ADAPTIVE_BUTTON;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.BACK;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.FORWARD;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.HOME;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.MENU;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.PADDING;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.RELOAD;
import static org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId.TAB_SWITCHER;

import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Looper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinatorTablet;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBarAnimatingView;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.browser.toolbar.incognito.IncognitoIndicatorCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils.ToolbarComponentId;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ToastManager;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for @{@link ToolbarTablet} */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(BaseRobolectricTestRunner.class)
public final class ToolbarTabletUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private LocationBarCoordinator mLocationBar;
    @Mock private LocationBarCoordinatorTablet mLocationBarTablet;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    @Mock private StatusCoordinator mStatusCoordinator;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private TabStripTransitionCoordinator mTabStripTransitionCoordinator;
    @Mock private ToolbarColorObserver mToolbarColorObserver;
    @Mock private ToolbarDataProvider mToolbarDataProvider;
    @Mock private ToolbarTabController mToolbarTabController;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private ReloadButtonCoordinator mReloadButtonCoordinator;
    @Mock private BackButtonCoordinator mBackButtonCoordinator;
    @Mock private HomeButtonCoordinator mHomeButtonCoordinator;
    @Mock private IncognitoIndicatorCoordinator mIncognitoIndicatorCoordinator;
    @Mock private ForwardButtonCoordinator mForwardButtonCoordinator;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private NavigationPopup.HistoryDelegate mHistoryDelegate;

    private Activity mActivity;
    private ToolbarTablet mToolbarTablet;
    private LinearLayout mToolbarTabletLayout;
    private ImageButton mHomeButton;
    private ImageButton mReloadingButton;
    private ImageButton mBackButton;
    private ChromeImageButton mForwardButton;
    private ImageButton mBookmarkButton;
    private ToolbarProgressBar mProgressBar;
    private Set<TintObserver> mTintObservers;
    private Set<IncognitoStateObserver> mIncognitoStateObservers;

    private final Answer<Object> mAddIncognitoObserverInIncognitoMode =
            (invocation) -> {
                IncognitoStateProvider.IncognitoStateObserver observer = invocation.getArgument(0);
                observer.onIncognitoStateChanged(/* isIncognito */ true);
                return null;
            };

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mTintObservers = new HashSet<>();
        doAnswer(
                        invocation -> {
                            mTintObservers.add((TintObserver) invocation.getArguments()[0]);
                            return null;
                        })
                .when(mThemeColorProvider)
                .addTintObserver(any());

        mIncognitoStateObservers = new HashSet<>();
        doAnswer(
                        invocation -> {
                            mIncognitoStateObservers.add(
                                    (IncognitoStateObserver) invocation.getArguments()[0]);
                            return null;
                        })
                .when(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(any());

        ToolbarTablet realView =
                (ToolbarTablet)
                        mActivity.getLayoutInflater().inflate(R.layout.toolbar_tablet, null);
        mToolbarTablet = Mockito.spy(realView);
        when(mLocationBar.getTabletCoordinator()).thenReturn(mLocationBarTablet);
        mToolbarTablet.setLocationBarCoordinator(mLocationBar);
        LocationBarLayout locationBarLayout = mToolbarTablet.findViewById(R.id.location_bar);
        locationBarLayout.setStatusCoordinatorForTesting(mStatusCoordinator);
        mToolbarTablet.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        mToolbarTablet.setTabStripTransitionCoordinator(mTabStripTransitionCoordinator);
        mToolbarTablet.setToolbarColorObserver(mToolbarColorObserver);
        mToolbarTablet.setReloadButtonCoordinator(mReloadButtonCoordinator);
        mToolbarTablet.setBackButtonCoordinator(mBackButtonCoordinator);
        mToolbarTablet.setHomeButtonWidthConsumerForTesting(mHomeButtonCoordinator);
        mToolbarTablet.setTabStackButtonCoordinatorForTesting(mTabSwitcherButtonCoordinator);
        mToolbarTablet.setIncognitoIndicatorCoordinatorForTesting(mIncognitoIndicatorCoordinator);
        mToolbarTablet.ensureOptionalButtonWidthConsumerForTesting();
        mToolbarTablet.ensurePaddingWidthConsumer();
        mToolbarTabletLayout = mToolbarTablet.findViewById(R.id.toolbar_tablet_layout);
        mHomeButton = mToolbarTablet.findViewById(R.id.home_button);
        mBackButton = mToolbarTablet.findViewById(R.id.back_button);
        mForwardButton = mToolbarTablet.findViewById(R.id.forward_button);
        mReloadingButton = mToolbarTablet.findViewById(R.id.refresh_button);
        mBookmarkButton = mToolbarTablet.findViewById(R.id.bookmark_button);
        mProgressBar = new ToolbarProgressBar(mActivity, null);
        mProgressBar.setAnimatingView(new ToolbarProgressBarAnimatingView(mActivity, null));
        when(mReloadButtonCoordinator.getFadeAnimator(false))
                .thenReturn(ObjectAnimator.ofFloat(mReloadingButton, View.ALPHA, 0.f));
        when(mReloadButtonCoordinator.getFadeAnimator(true))
                .thenReturn(ObjectAnimator.ofFloat(mReloadingButton, View.ALPHA, 1.f));
        when(mBackButtonCoordinator.getFadeAnimator(false))
                .thenReturn(ObjectAnimator.ofFloat(mReloadingButton, View.ALPHA, 0.f));
        when(mBackButtonCoordinator.getFadeAnimator(true))
                .thenReturn(ObjectAnimator.ofFloat(mReloadingButton, View.ALPHA, 1.f));

        int buttonWidth =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);

        doReturn(buttonWidth).when(mHomeButtonCoordinator).updateVisibility(geq(buttonWidth));
        doReturn(0).when(mHomeButtonCoordinator).updateVisibility(lt(buttonWidth));

        doReturn(buttonWidth).when(mReloadButtonCoordinator).updateVisibility(geq(buttonWidth));
        doReturn(0).when(mReloadButtonCoordinator).updateVisibility(lt(buttonWidth));

        doReturn(buttonWidth).when(mBackButtonCoordinator).updateVisibility(geq(buttonWidth));
        doReturn(0).when(mBackButtonCoordinator).updateVisibility(lt(buttonWidth));

        doReturn(buttonWidth)
                .when(mTabSwitcherButtonCoordinator)
                .updateVisibility(geq(buttonWidth));
        doReturn(0).when(mTabSwitcherButtonCoordinator).updateVisibility(lt(buttonWidth));

        doReturn(buttonWidth).when(mMenuButtonCoordinator).updateVisibility(geq(buttonWidth));
        doReturn(0).when(mMenuButtonCoordinator).updateVisibility(lt(buttonWidth));

        mForwardButtonCoordinator =
                new ForwardButtonCoordinator(
                        mActivity,
                        mToolbarDataProvider,
                        mToolbarTabController,
                        () -> mLocationBar,
                        mActivityLifecycleDispatcher,
                        mForwardButton,
                        mHistoryDelegate,
                        mThemeColorProvider,
                        mIncognitoStateProvider);
        mToolbarTablet.setForwardButtonCoordinatorForTesting(mForwardButtonCoordinator);
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

    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @Test
    public void testButtonPositionIncognito() {
        doAnswer(mAddIncognitoObserverInIncognitoMode)
                .when(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(any());

        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                null,
                null,
                mProgressBar,
                mReloadButtonCoordinator,
                mBackButtonCoordinator,
                mForwardButtonCoordinator,
                /* homeButtonDisplay= */ null,
                /* extensionToolbarCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider);
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

        assertEquals(
                "Toolbar button visibility is not as expected",
                View.GONE,
                mForwardButton.getVisibility());

        verify(mReloadButtonCoordinator).setVisibility(false);
        verify(mBackButtonCoordinator).setVisibility(false);
    }

    @Test
    public void onMeasureLargeWidth_showsToolbarButtons() {
        mToolbarTablet.setToolbarButtonsVisibleForTesting(false);
        mToolbarTablet.measure(700, 300);

        assertEquals(
                "Toolbar button visibility is not as expected",
                View.VISIBLE,
                mForwardButton.getVisibility());

        verify(mReloadButtonCoordinator).setVisibility(true);
        verify(mBackButtonCoordinator).setVisibility(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void onMeasureIncognito_flipIncognitoVisibility() {
        doAnswer(mAddIncognitoObserverInIncognitoMode)
                .when(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(any());

        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                null,
                null,
                mProgressBar,
                mReloadButtonCoordinator,
                mBackButtonCoordinator,
                mForwardButtonCoordinator,
                /* homeButtonDisplay= */ null,
                null,
                mThemeColorProvider,
                mIncognitoStateProvider);
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

        when(mLocationBar.createHideButtonAnimatorForTablet(mForwardButton))
                .thenReturn(ObjectAnimator.ofFloat(mForwardButton, View.ALPHA, 0.f));
        when(mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(new ArrayList<>());

        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();
        // Call
        mToolbarTablet.measure(300, 300);
        verify(mTabStripTransitionCoordinator).requestDeferTabStripTransitionToken();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        assertEquals(
                "Toolbar button visibility is not as expected",
                View.GONE,
                mForwardButton.getVisibility());
        verify(mReloadButtonCoordinator).setVisibility(false);
        verify(mTabStripTransitionCoordinator, atLeastOnce()).releaseTabStripToken(anyInt());
    }

    @Test
    public void onMeasureLargeWidthWithAnimation_showsToolbarButtons() {
        doReturn(true).when(mToolbarTablet).isShown();
        mToolbarTablet.setToolbarButtonsVisibleForTesting(false);
        mToolbarTablet.enableButtonVisibilityChangeAnimationForTesting();

        when(mLocationBar.createShowButtonAnimatorForTablet(mForwardButton))
                .thenReturn(ObjectAnimator.ofFloat(mForwardButton, View.ALPHA, 1.f));
        when(mLocationBar.getShowButtonsWhenUnfocusedAnimatorsForTablet(anyInt()))
                .thenReturn(new ArrayList<>());
        // Call
        mToolbarTablet.measure(700, 300);
        verify(mTabStripTransitionCoordinator).requestDeferTabStripTransitionToken();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        // Verify
        assertEquals(
                "Toolbar button visibility is not as expected",
                View.VISIBLE,
                mForwardButton.getVisibility());
        verify(mReloadButtonCoordinator).setVisibility(true);
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
    public void testUpdateForwardButtonVisibility() {
        mToolbarTablet.onFinishInflate();
        mToolbarTablet.initialize(
                mToolbarDataProvider,
                null,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                null,
                null,
                null,
                mProgressBar,
                mReloadButtonCoordinator,
                mBackButtonCoordinator,
                mForwardButtonCoordinator,
                /* homeButtonDisplay= */ null,
                /* extensionToolbarCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider);
        ImageButton btn = mToolbarTablet.findViewById(R.id.forward_button);

        doReturn(null).when(mToolbarDataProvider).getTab();
        mToolbarTablet.updateButtonVisibility();
        assertFalse("Button should not be enabled", btn.isEnabled());
        assertFalse("Button should not be focused", btn.isFocusable());

        Tab tab = Mockito.mock(Tab.class);
        doReturn(false).when(tab).canGoForward();
        doReturn(tab).when(mToolbarDataProvider).getTab();
        mToolbarTablet.updateButtonVisibility();
        assertFalse("Button should not be enabled", btn.isEnabled());
        assertFalse("Button should not be focused", btn.isFocusable());

        doReturn(true).when(tab).canGoForward();
        mToolbarTablet.updateButtonVisibility();
        assertTrue("Button should be enabled", btn.isEnabled());
        assertTrue("Button should be focused", btn.isFocusable());
    }

    @Test
    public void testIsReadyForTextureCapture_HasFocus() {
        mToolbarTablet.onUrlFocusChange(/* hasFocus= */ true);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS, result.blockReason);
    }

    @Test
    public void testOptionalButtonTooltipText() {
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.READER_MODE,
                /* tooltipTextResId= */ 0);
        Assert.assertEquals(
                null, mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether share button tooltip Text is set correctly.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                R.string.adaptive_toolbar_button_preference_share);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_share),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether voice search button tooltip Text is set correctly.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.VOICE,
                R.string.adaptive_toolbar_button_preference_voice_search);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.adaptive_toolbar_button_preference_voice_search),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());

        // Test whether new tab button tooltip Text is set correctly.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.NEW_TAB, R.string.new_tab_title);
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.new_tab_title),
                mToolbarTablet.getOptionalButtonViewForTesting().getTooltipText());
    }

    @Test
    public void testOptionalButtonWithErrorBadge() {
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.READER_MODE,
                /* tooltipTextResId= */ 0);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.optional_toolbar_tablet_button_padding_start),
                mToolbarTablet.getOptionalButtonViewForTesting().getPaddingStart());
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.optional_toolbar_tablet_button_padding_top),
                mToolbarTablet.getOptionalButtonViewForTesting().getPaddingTop());

        // Test whether the paddings are set correctly.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                /* tooltipTextResId= */ 0,
                /* hasErrorBadge= */ true);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen
                                        .optional_toolbar_tablet_button_with_error_badge_padding_start),
                mToolbarTablet.getOptionalButtonViewForTesting().getPaddingStart());
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen
                                        .optional_toolbar_tablet_button_with_error_badge_padding_top),
                mToolbarTablet.getOptionalButtonViewForTesting().getPaddingTop());
    }

    @Test
    public void testIsReadyForTextureCapture_InTabSwitcher() {
        mToolbarTablet.setTabSwitcherMode(/* inTabSwitcherMode= */ true);
        CaptureReadinessResult result = mToolbarTablet.isReadyForTextureCapture();
        Assert.assertFalse(result.isReady);
        Assert.assertEquals(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE, result.blockReason);
    }

    @Test
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
        doReturn(tint).when(mThemeColorProvider).getTint();
        doReturn(tint).when(mThemeColorProvider).getActivityFocusTint();

        // Setup.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.NEW_TAB,
                R.string.adaptive_toolbar_button_preference_new_tab);
        for (TintObserver observer : mTintObservers) {
            observer.onTintChanged(tint, tint, BrandedColorScheme.APP_DEFAULT);
        }

        // Verify the toolbar icon tints, assuming that the activity is initially focused.
        verifyToolbarIconTints(tint, tint);

        // Simulate a tint change triggered when the activity loses focus.
        for (TintObserver observer : mTintObservers) {
            observer.onTintChanged(tint, unfocusedTint, BrandedColorScheme.APP_DEFAULT);
        }
        mToolbarTablet.onTintChanged(tint, unfocusedTint, BrandedColorScheme.APP_DEFAULT);

        // Verify the icon tints for the unfocused activity.
        verifyToolbarIconTints(tint, unfocusedTint);
    }

    @SuppressLint("WrongCall")
    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testResizeTabletToolbar_buttonsShowInRankedOrder() {
        int widthForStaticComponents = mToolbarTablet.getWidthForStaticComponents();
        int buttonWidth =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        int padding =
                mToolbarTablet
                                .getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tablet_toolbar_start_padding)
                        * 2;
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                R.string.adaptive_toolbar_button_preference_share);

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(widthForStaticComponents, EXACTLY), UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of());

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(2 * buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(BACK, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(3 * buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(BACK, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        3 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(PADDING, BACK, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        4 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        5 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        6 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        7 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        8 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));
    }

    @SuppressLint("WrongCall")
    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testResizeTabletToolbar_buttonsHideInRankedOrder() {
        int widthForStaticComponents = mToolbarTablet.getWidthForStaticComponents();
        int buttonWidth =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        int padding =
                mToolbarTablet
                                .getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tablet_toolbar_start_padding)
                        * 2;
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                R.string.adaptive_toolbar_button_preference_share);

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        8 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        7 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        6 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        5 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        4 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, BACK, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(
                        3 * buttonWidth + widthForStaticComponents + padding, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(PADDING, BACK, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(3 * buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(BACK, TAB_SWITCHER, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(2 * buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(BACK, MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(buttonWidth + widthForStaticComponents, EXACTLY),
                UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of(MENU));

        mToolbarTablet.onMeasure(
                MeasureSpec.makeMeasureSpec(widthForStaticComponents, EXACTLY), UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(Set.of());
    }

    @SuppressLint("WrongCall")
    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testResizeTabletToolbar_toggleOptionalButtonHidden() {
        int widthForStaticComponents = mToolbarTablet.getWidthForStaticComponents();
        int buttonWidth =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        int padding =
                mToolbarTablet
                                .getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tablet_toolbar_start_padding)
                        * 2;
        int toolbarWidth = 8 * buttonWidth + widthForStaticComponents + padding;
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                R.string.adaptive_toolbar_button_preference_share);

        doReturn(toolbarWidth).when(mToolbarTablet).getWidth();

        // The optional / adaptive button should be showing.
        mToolbarTablet.onMeasure(MeasureSpec.makeMeasureSpec(toolbarWidth, EXACTLY), UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));

        // The optional / adaptive button should stop showing.
        mToolbarTablet.hideOptionalButton();
        mToolbarTablet.onMeasure(MeasureSpec.makeMeasureSpec(toolbarWidth, EXACTLY), UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, TAB_SWITCHER, MENU));

        // The optional / adaptive button should start showing again after being updated.
        updateOptionalButton(
                /* buttonVariant= */ AdaptiveToolbarButtonVariant.SHARE,
                R.string.adaptive_toolbar_button_preference_share);
        mToolbarTablet.onMeasure(MeasureSpec.makeMeasureSpec(toolbarWidth, EXACTLY), UNSPECIFIED);
        assertToolbarComponentsReceivedWidth(
                Set.of(PADDING, HOME, BACK, FORWARD, RELOAD, ADAPTIVE_BUTTON, TAB_SWITCHER, MENU));
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void assertToolbarComponentsReceivedWidth(
            @ToolbarComponentId Set<Integer> visibleComponents) {
        int buttonWidth =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);
        int padding =
                mToolbarTablet
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tablet_toolbar_start_padding);

        if (visibleComponents.contains(HOME)) {
            verify(mHomeButtonCoordinator).updateVisibility(geq(buttonWidth));
        } else {
            verify(mHomeButtonCoordinator, never()).updateVisibility(geq(buttonWidth));
        }

        if (visibleComponents.contains(BACK)) {
            verify(mBackButtonCoordinator).updateVisibility(geq(buttonWidth));
        } else {
            verify(mBackButtonCoordinator, never()).updateVisibility(geq(buttonWidth));
        }

        if (visibleComponents.contains(RELOAD)) {
            verify(mReloadButtonCoordinator).updateVisibility(geq(buttonWidth));
        } else {
            verify(mReloadButtonCoordinator, never()).updateVisibility(geq(buttonWidth));
        }

        if (visibleComponents.contains(TAB_SWITCHER)) {
            verify(mTabSwitcherButtonCoordinator).updateVisibility(geq(buttonWidth));
        } else {
            verify(mTabSwitcherButtonCoordinator, never()).updateVisibility(geq(buttonWidth));
        }

        if (visibleComponents.contains(MENU)) {
            verify(mMenuButtonCoordinator).updateVisibility(geq(buttonWidth));
        } else {
            verify(mMenuButtonCoordinator, never()).updateVisibility(geq(buttonWidth));
        }

        Mockito.clearInvocations(
                mHomeButtonCoordinator,
                mBackButtonCoordinator,
                mReloadButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                mMenuButtonCoordinator);

        // Replace with a mock when the ForwardButtonCoordinator has its own unit tests.
        assertEquals(
                visibleComponents.contains(FORWARD) ? View.VISIBLE : View.GONE,
                mForwardButtonCoordinator.getButton().getVisibility());

        assertEquals(
                visibleComponents.contains(ADAPTIVE_BUTTON) ? View.VISIBLE : View.GONE,
                mToolbarTablet.getOptionalButtonViewForTesting().getVisibility());

        if (visibleComponents.contains(PADDING)) {
            assertEquals(padding, mToolbarTablet.getPaddingStart());
            assertEquals(padding, mToolbarTablet.getPaddingEnd());
        } else {
            assertEquals(0, mToolbarTablet.getPaddingStart());
            assertEquals(0, mToolbarTablet.getPaddingEnd());
        }
    }

    private void verifyToolbarIconTints(ColorStateList tint, ColorStateList activityFocusTint) {
        Assert.assertEquals(
                "Home button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mHomeButton.getImageTintList().getDefaultColor());
        Assert.assertEquals(
                "Forward button tint is incorrect.",
                activityFocusTint.getDefaultColor(),
                mForwardButton.getImageTintList().getDefaultColor());
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

    private void updateOptionalButton(
            @AdaptiveToolbarButtonVariant int buttonVariant, int tooltipTextResId) {
        updateOptionalButton(buttonVariant, tooltipTextResId, /* hasErrorBadge= */ false);
    }

    private void updateOptionalButton(
            @AdaptiveToolbarButtonVariant int buttonVariant,
            int tooltipTextResId,
            boolean hasErrorBadge) {
        // Verify reader mode tooltip text is null.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        AppCompatResources.getDrawable(mActivity, R.drawable.new_tab_icon),
                        v -> {},
                        v -> false,
                        mActivity.getString(R.string.actionbar_share),
                        true,
                        null,
                        buttonVariant,
                        0,
                        tooltipTextResId,
                        hasErrorBadge);

        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        mToolbarTablet.updateOptionalButton(buttonData);
    }
}
