// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingEvent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.IphEvent;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomBarEducationCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true"})
public class BottomBarEducationCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ScrimManager mScrimManager;
    @Mock private BrowserControlsVisibilityManager mVisibilityManager;
    private BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;

    private Activity mActivity;
    private Context mContext;
    private BottomBarView mBottomBarView;
    private View mControlContainer;
    private BottomBarButtonContainer mGlicBtn;
    private BottomBarEducationCoordinator mCoordinator;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mContext = mActivity;

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);

        mVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.alwaysFalse());
        when(mVisibilityManager.getBrowserVisibilityDelegate()).thenReturn(mVisibilityDelegate);
        when(mVisibilityManager.getControlsPosition()).thenReturn(ControlsPosition.TOP);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(true);
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        TrackerFactory.setTrackerForTests(null);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(null);
    }

    @Test
    public void testWouldShow_ReturnsTrueWhenTrackerTriggers() {
        setupForShow();
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);
        assertTrue(mCoordinator.wouldShow(mProfile));

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);
        assertFalse(mCoordinator.wouldShow(mProfile));
    }

    @Test
    public void testEducationShownThisSession_IsFalseBeforeBubbleShows() {
        setupForShow();
        assertFalse(BottomBarEducationCoordinator.hasEducationShownThisSession());
        triggerShow();
        assertTrue(BottomBarEducationCoordinator.hasEducationShownThisSession());
    }

    @Test
    public void testShouldTriggerHelpUiFalse_DoesNotShow() {
        setupForShow();
        assertFalse(BottomBarEducationCoordinator.hasEducationShownThisSession());
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);
        triggerShow();

        verify(mScrimManager, never()).showScrim(any());
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        assertNull(mCoordinator.getBubbleViewForTesting());
        assertFalse(BottomBarEducationCoordinator.hasEducationShownThisSession());
    }

    @Test
    public void testDestroyWhileShowingBubble() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.DISMISSED_OTHER)
                        .build();
        setupForShow();
        triggerShow();
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        assertTrue(BottomBarEducationCoordinator.hasEducationShownThisSession());
        assertEquals(BrowserControlsState.SHOWN, mVisibilityDelegate.get().intValue());
        mCoordinator.destroy();
        verify(mScrimManager).hideScrim(any(), anyBoolean());
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        watcher.assertExpected();
    }

    @Test
    public void testSwitchBackDismissesAndTurnsOffBottomBar() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.DECLINED)
                        .build();
        SettingsNavigation settingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(settingsNavigation);

        setupForShow();
        triggerShow();
        assertTrue(isBubbleShowing());

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.BOTTOM_BAR_ENABLED));

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        View switchBackBtn = bubbleView.findViewById(R.id.negative_button);
        switchBackBtn.performClick();
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.BOTTOM_BAR_ENABLED));
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        verifyNoInteractions(settingsNavigation);
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        assertFalse(isBubbleShowing());
        watcher.assertExpected();
    }

    @Test
    public void testDismissInIdleNeverCallsTracker() {
        setupForShow();
        // Do not trigger show, stay IDLE
        mCoordinator.dismiss();

        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    public void testDismissNotifiesTrackerOnce() {
        setupForShow();
        triggerShow();

        mCoordinator.dismiss();
        mCoordinator.dismiss();

        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    public void testBarNotLaidOut_WaitsForLayout() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .build();
        setupForShow();
        mBottomBarView.setVisibility(View.INVISIBLE);
        mCoordinator.maybeShow();

        // Not shown yet
        verify(mScrimManager, never()).showScrim(any());

        // Simulate becoming visible and laid out
        mBottomBarView.setVisibility(View.VISIBLE);
        mBottomBarView.layout(0, 0, 1080, 201);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        // Now shown
        verify(mScrimManager).showScrim(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAncestorBecomesVisibleWithoutRelayout_Shows() {
        setupForShow();
        View bottomAppBarContainer = (View) mBottomBarView.getParent();
        bottomAppBarContainer.setVisibility(View.INVISIBLE);
        mCoordinator.maybeShow();
        verify(mScrimManager, never()).showScrim(any());

        // The bottom controls make the container visible without laying the bar out again.
        bottomAppBarContainer.setVisibility(View.VISIBLE);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mScrimManager).showScrim(any());
        assertTrue(isBubbleShowing());
    }

    @Test
    public void testDestroy_WhileWaitingForLayout_RemovesListener() {
        setupForShow();
        mBottomBarView.setVisibility(View.INVISIBLE);
        mCoordinator.maybeShow();

        // Destroy while waiting
        mCoordinator.destroy();
        mCoordinator = null;

        // Dispatch layout, should not trigger anything
        mBottomBarView.setVisibility(View.VISIBLE);
        mBottomBarView.layout(0, 0, 1080, 201);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    public void testTrackerNotInitialized_WaitsForCallback() {
        setupForShow();
        when(mTracker.isInitialized()).thenReturn(false);
        mCoordinator.maybeShow();

        // Not shown yet
        verify(mScrimManager, never()).showScrim(any());

        // Simulate callback
        @SuppressWarnings("unchecked")
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mTracker).addOnInitializedCallback(callbackCaptor.capture());
        when(mTracker.isInitialized()).thenReturn(true);
        callbackCaptor.getValue().onResult(true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mScrimManager).showScrim(any());
    }

    @Test
    public void testComputeArrowOffset() {
        // popup x=48, width=984, anchor center=540
        assertEquals(492, BottomBarEducationCoordinator.computeArrowOffset(540, 48, 984, 10, 20));
        assertEquals(10, BottomBarEducationCoordinator.computeArrowOffset(5, 0, 100, 10, 20));
        assertEquals(80, BottomBarEducationCoordinator.computeArrowOffset(95, 0, 100, 10, 20));
        assertEquals(40, BottomBarEducationCoordinator.computeArrowOffset(40, 0, 100, 10, 20));
    }

    @Test
    public void testSelectAnchorTarget() {
        View bar = new View(mActivity);
        View glic = new View(mActivity);

        // gemini+glic -> glic
        assertEquals(glic, BottomBarEducationCoordinator.selectAnchorTarget(true, glic, bar));

        // gemini+null -> bar
        assertEquals(bar, BottomBarEducationCoordinator.selectAnchorTarget(true, null, bar));

        // default -> bar
        assertEquals(bar, BottomBarEducationCoordinator.selectAnchorTarget(false, glic, bar));
    }

    @Test
    public void testShouldUseGeminiVariant() {
        setupForShow();
        View glic = mGlicBtn;

        assertFalse(BottomBarEducationCoordinator.shouldUseGeminiVariant(false, glic));

        assertFalse(BottomBarEducationCoordinator.shouldUseGeminiVariant(true, null));

        glic.setVisibility(View.GONE);
        assertFalse(BottomBarEducationCoordinator.shouldUseGeminiVariant(true, glic));

        glic.setVisibility(View.VISIBLE);
        glic.layout(0, 0, 0, 100);
        assertFalse(BottomBarEducationCoordinator.shouldUseGeminiVariant(true, glic));

        glic.layout(0, 0, 100, 100);
        assertTrue(BottomBarEducationCoordinator.shouldUseGeminiVariant(true, glic));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":education_enabled/true/education_gemini_focused/true")
    public void testGeminiParamWithoutGlicButtonShowsDefaultCopy() {
        setupForShow();
        // Hide GLIC button
        mBottomBarView.setButtonVisibility(ActionId.GLIC, false);

        triggerShow();
        assertTrue(isBubbleShowing());

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        TextView title = bubbleView.findViewById(R.id.education_title);

        // It should fall back to default copy because GLIC is hidden
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_title_ergonomics),
                title.getText().toString());
    }

    @Test
    public void testSettingsLinkOpensSettings() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SETTINGS_CLICKED)
                        .build();
        SettingsNavigation settingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(settingsNavigation);
        setupForShow();
        triggerShow();
        assertTrue(isBubbleShowing());

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        TextView body = bubbleView.findViewById(R.id.education_body);

        // Find the clickable span and click it
        SpannableString spannable = (SpannableString) body.getText();
        ClickableSpan[] spans = spannable.getSpans(0, spannable.length(), ClickableSpan.class);
        assertEquals(1, spans.length);

        spans[0].onClick(body);

        verify(settingsNavigation)
                .startSettings(mContext, SettingsNavigation.SettingsFragment.APPEARANCE);
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        assertFalse(isBubbleShowing());
        watcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "land")
    public void testReShowOnRotation() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN,
                                EducationOnboardingEvent.DISMISSED_OTHER)
                        .build();
        setupForShow();
        triggerShow();

        // Should be called initially
        verify(mScrimManager, times(1)).showScrim(any());
        assertEquals(BrowserControlsState.SHOWN, mVisibilityDelegate.get().intValue());

        View firstBubble = mCoordinator.getBubbleViewForTesting();

        // Simulate rotation to portrait
        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        newConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        mCoordinator.onConfigurationChanged(newConfig);

        assertFalse(isBubbleShowing());

        // Wait for layout changes to propagate
        mBottomBarView.layout(0, 0, 200, 200);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        // Should NOT trigger tracker dismissed
        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);

        // Assert popup shown again (getBubbleViewForTesting)
        assertTrue(isBubbleShowing());
        assertNotSame(firstBubble, mCoordinator.getBubbleViewForTesting());

        // Scrim and token should NOT be requested again on re-show!
        verify(mScrimManager, times(1)).showScrim(any());
        assertEquals(BrowserControlsState.SHOWN, mVisibilityDelegate.get().intValue());

        mCoordinator.dismiss();
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testIgnoredConfigChange() {
        setupForShow();
        triggerShow();

        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        mCoordinator.onConfigurationChanged(newConfig);

        // Flow shouldn't be disrupted
        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        assertTrue(isBubbleShowing());
    }

    @Test
    public void testOnControlsPositionChangedDismisses() {
        setupForShow();
        triggerShow();

        mCoordinator.onControlsPositionChanged(ControlsPosition.BOTTOM);

        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    public void testComputeBubbleAnchorTop_ClearsBottomOmnibox() {
        setupForShow();
        // Stack the omnibox directly above the bottom bar, as when the controls are at the bottom.
        mControlContainer.layout(0, 1600, 1080, 1800);
        ((View) mBottomBarView.getParent()).layout(0, 1800, 1080, 2000);
        int[] rootLocation = new int[2];
        ((View) mControlContainer.getParent()).getLocationInWindow(rootLocation);
        int omniboxTop = rootLocation[1] + 1600;
        int bottomBarTop = rootLocation[1] + 1800;

        assertEquals(
                omniboxTop,
                BottomBarEducationCoordinator.computeBubbleAnchorTop(
                        ControlsPosition.BOTTOM, mBottomBarView, mControlContainer));
        assertEquals(
                bottomBarTop,
                BottomBarEducationCoordinator.computeBubbleAnchorTop(
                        ControlsPosition.TOP, mBottomBarView, mControlContainer));
        assertEquals(
                bottomBarTop,
                BottomBarEducationCoordinator.computeBubbleAnchorTop(
                        ControlsPosition.BOTTOM, mBottomBarView, null));
    }

    @Test
    public void testObserversRegisteredOnlyWhileShowing() {
        setupForShow();
        verify(mVisibilityManager, never()).addObserver(any());

        triggerShow();
        verify(mVisibilityManager).addObserver(mCoordinator);

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        if (bubbleView != null) {
            allowTooltipsToShowInRobolectric();
            bubbleView.findViewById(R.id.positive_button).performClick();
            verify(mVisibilityManager, never()).removeObserver(any());
        }

        mCoordinator.dismiss();
        verify(mVisibilityManager).removeObserver(mCoordinator);
    }

    @Test
    public void testTooltipChain_AllButtons() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.ACCEPTED)
                        .expectIntRecord("Android.BottomBar.IPH.Glic.Event", IphEvent.SHOWN)
                        .expectIntRecord("Android.BottomBar.IPH.Glic.Event", IphEvent.DISMISSED)
                        .expectIntRecord("Android.BottomBar.IPH.NewTab.Event", IphEvent.SHOWN)
                        .expectIntRecord("Android.BottomBar.IPH.NewTab.Event", IphEvent.DISMISSED)
                        .expectIntRecord("Android.BottomBar.IPH.TabSwitcher.Event", IphEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.IPH.TabSwitcher.Event", IphEvent.DISMISSED)
                        .build();
        setupForShow();
        allowTooltipsToShowInRobolectric();
        triggerShow();

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        bubbleView.findViewById(R.id.positive_button).performClick();
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip1 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip1);
        assertTrue(tooltip1.getDismissOnTouchInteractionForTesting());

        tooltip1.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip2 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip2);
        assertTrue(tooltip2.getDismissOnTouchInteractionForTesting());
        assertNotSame(tooltip1, tooltip2);

        tooltip2.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip3 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip3);
        assertTrue(tooltip3.getDismissOnTouchInteractionForTesting());
        assertNotSame(tooltip2, tooltip3);

        tooltip3.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(mCoordinator.getCurrentTooltipForTesting());
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testTooltipChain_SkipsHiddenButtons() {
        setupForShow();
        mBottomBarView.setButtonVisibility(ActionId.NEW_TAB, false);
        allowTooltipsToShowInRobolectric();
        triggerShow();

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        bubbleView.findViewById(R.id.positive_button).performClick();
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip1 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip1);
        assertTrue(tooltip1.getDismissOnTouchInteractionForTesting());

        tooltip1.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip3 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip3);
        assertTrue(tooltip3.getDismissOnTouchInteractionForTesting());
        assertNotSame(tooltip1, tooltip3);

        tooltip3.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(mCoordinator.getCurrentTooltipForTesting());
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    @Config(qualifiers = "land")
    public void testRotationDuringTooltipsReshowsCurrentTooltip() {
        setupForShow();
        allowTooltipsToShowInRobolectric();
        triggerShow();

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        bubbleView.findViewById(R.id.positive_button).performClick();
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip1 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip1);
        assertTrue(tooltip1.getDismissOnTouchInteractionForTesting());

        tooltip1.onDismissForTesting(false);
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble tooltip2 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip2);
        assertTrue(tooltip2.getDismissOnTouchInteractionForTesting());

        // Simulate a configuration change during tooltips.
        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        newConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        mCoordinator.onConfigurationChanged(newConfig);

        assertNull(mCoordinator.getCurrentTooltipForTesting());

        mBottomBarView.layout(0, 0, 1080, 2000);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble newTooltip2 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(newTooltip2);
        assertNotSame(tooltip2, newTooltip2);
        // Verify it really is the 2nd tooltip (New Tab).
        TextView msgView =
                newTooltip2.getTextBubbleContentViewForTesting().findViewById(R.id.message);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_tooltip_new_tab),
                msgView.getText().toString());

        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    public void testDestroyWhileShowingTooltips() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.DefaultBubble.Event",
                                EducationOnboardingEvent.ACCEPTED)
                        .expectIntRecord("Android.BottomBar.IPH.Glic.Event", IphEvent.SHOWN)
                        .expectIntRecord("Android.BottomBar.IPH.Glic.Event", IphEvent.DISMISSED)
                        .build();
        setupForShow();
        allowTooltipsToShowInRobolectric();
        triggerShow();

        View bubbleView = mCoordinator.getBubbleViewForTesting();
        bubbleView.findViewById(R.id.positive_button).performClick();

        mCoordinator.destroy();

        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        watcher.assertExpected();
    }

    @Test
    public void testWouldShow_ReturnsTrueWhenPendingTooltips() {
        setupForShow();
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);
        setPendingTooltips();
        assertTrue(mCoordinator.wouldShow(mProfile));
    }

    @Test
    public void testPendingTooltips_StartsTooltipChainWithoutBubble() {
        setupForShow();
        allowTooltipsToShowInRobolectric();
        // The hand-over does not depend on the tracker.
        when(mTracker.isInitialized()).thenReturn(false);
        setPendingTooltips();

        triggerShow();

        assertTrue(BottomBarEducationCoordinator.hasEducationShownThisSession());
        TextBubble tooltip = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip);
        assertTrue(tooltip.getDismissOnTouchInteractionForTesting());
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_tooltip_gemini),
                getTooltipText(tooltip));
        assertNull(mCoordinator.getBubbleViewForTesting());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.BOTTOM_BAR_EDUCATION_PENDING_TOOLTIPS));
        verify(mScrimManager, never()).showScrim(any());
        verify(mTracker, never()).shouldTriggerHelpUi(any());
        verify(mVisibilityManager).addObserver(mCoordinator);
        assertEquals(BrowserControlsState.SHOWN, mVisibilityDelegate.get().intValue());

        mCoordinator.dismiss();
        verify(mVisibilityManager).removeObserver(mCoordinator);
        assertEquals(BrowserControlsState.BOTH, mVisibilityDelegate.get().intValue());
        verify(mTracker, never()).dismissed(any());
    }

    @Test
    @Config(qualifiers = "land")
    public void testPendingTooltips_RotationReshowsCurrentTooltip() {
        setupForShow();
        allowTooltipsToShowInRobolectric();
        setPendingTooltips();
        triggerShow();

        TextBubble tooltip1 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(tooltip1);

        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        newConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        mCoordinator.onConfigurationChanged(newConfig);
        assertNull(mCoordinator.getCurrentTooltipForTesting());

        mBottomBarView.layout(0, 0, 1080, 2000);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        TextBubble newTooltip1 = mCoordinator.getCurrentTooltipForTesting();
        assertNotNull(newTooltip1);
        assertNotSame(tooltip1, newTooltip1);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_tooltip_gemini),
                getTooltipText(newTooltip1));
        assertEquals(BrowserControlsState.SHOWN, mVisibilityDelegate.get().intValue());
    }

    @Test
    @Config(qualifiers = "land")
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":education_enabled/true/education_gemini_focused/true")
    public void testNullProfileWhenShowingBubble_UsesDefaultCopy() {
        setupForShow();
        triggerShow();
        TextView title = mCoordinator.getBubbleViewForTesting().findViewById(R.id.education_title);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_title_gemini),
                title.getText().toString());

        // The bubble is re-created on rotation, after the profile went away.
        mProfile = null;
        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        newConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        mCoordinator.onConfigurationChanged(newConfig);
        mBottomBarView.layout(0, 0, 200, 200);
        mBottomBarView.getViewTreeObserver().dispatchOnGlobalLayout();
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(isBubbleShowing());
        title = mCoordinator.getBubbleViewForTesting().findViewById(R.id.education_title);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_title_ergonomics),
                title.getText().toString());
    }

    private boolean isBubbleShowing() {
        return mCoordinator.getBubbleViewForTesting() != null;
    }

    private void setPendingTooltips() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_EDUCATION_PENDING_TOOLTIPS, true);
    }

    private static String getTooltipText(TextBubble tooltip) {
        TextView msgView = tooltip.getTextBubbleContentViewForTesting().findViewById(R.id.message);
        return msgView.getText().toString();
    }

    private void allowTooltipsToShowInRobolectric() {
        TextBubble.setSkipShowCheckForTesting(true);
        BottomBarEducationCoordinator.setSkipTooltipShowCheckForTesting(true);
    }

    private void setupForShow() {
        FrameLayout root = new FrameLayout(mContext);
        mControlContainer = new FrameLayout(mContext);
        FrameLayout bottomAppBarContainer = new FrameLayout(mContext);
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.bottom_bar_layout, bottomAppBarContainer, false);
        mBottomBarView.inflateAllStubsForTesting();
        mBottomBarView.setButtonVisibility(ActionId.GLIC, true);
        mBottomBarView.setButtonVisibility(ActionId.NEW_TAB, true);
        mBottomBarView.setButtonVisibility(ActionId.TAB_SWITCHER, true);
        mBottomBarView.setNewTabBackgroundVisible(true);

        root.addView(mControlContainer, new FrameLayout.LayoutParams(1080, 200));
        root.addView(bottomAppBarContainer, new FrameLayout.LayoutParams(1080, 200));
        bottomAppBarContainer.addView(mBottomBarView, new FrameLayout.LayoutParams(1080, 200));

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            activity.setContentView(root);
                            root.measure(
                                    View.MeasureSpec.makeMeasureSpec(
                                            1080, View.MeasureSpec.EXACTLY),
                                    View.MeasureSpec.makeMeasureSpec(
                                            2000, View.MeasureSpec.EXACTLY));
                            root.layout(0, 0, 1080, 2000);
                        });

        mGlicBtn = mBottomBarView.getContainerForAction(ActionId.GLIC);

        mCoordinator =
                new BottomBarEducationCoordinator(
                        mScrimManager,
                        mVisibilityManager,
                        mControlContainer,
                        bottomAppBarContainer,
                        mBottomBarView,
                        () -> mProfile);
    }

    private void triggerShow() {
        mCoordinator.maybeShow();
        RobolectricUtil.runAllBackgroundAndUi();
    }
}
