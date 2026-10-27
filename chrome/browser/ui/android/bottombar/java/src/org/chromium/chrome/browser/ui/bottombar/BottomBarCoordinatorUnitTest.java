// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionProperties;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link BottomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ANDROID_BOTTOM_BAR})
public class BottomBarCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActionRegistry mActionRegistry;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private Tab mTab;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private ScrimManager mScrimManager;
    @Mock private GlicKeyedService mGlicKeyedService;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mHomeActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mMenuActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mTabSwitcherActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mGlicActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private FrameLayout mParent;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNonNullObservableSupplier<@ControlsPosition Integer> mToolbarPositionSupplier;
    private BottomBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mActionRegistry.get(ActionId.NEW_TAB)).thenReturn(mActionSupplier);
        when(mActionRegistry.get(ActionId.HOME_BUTTON)).thenReturn(mHomeActionSupplier);
        when(mActionRegistry.get(ActionId.APP_MENU)).thenReturn(mMenuActionSupplier);
        when(mActionRegistry.get(ActionId.TAB_SWITCHER)).thenReturn(mTabSwitcherActionSupplier);
        when(mActionRegistry.get(ActionId.GLIC)).thenReturn(mGlicActionSupplier);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mParent = new FrameLayout(mActivity);
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(true);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mToolbarPositionSupplier = ObservableSuppliers.createNonNull(ControlsPosition.TOP);
        mCoordinator = createCoordinator();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private BottomBarCoordinator createCoordinator() {
        return new BottomBarCoordinator(
                mParent,
                mActionRegistry,
                mThemeColorProvider,
                mTabSupplier,
                mHomepageEnabledSupplier,
                mVisibilityDelegate,
                mProfileSupplier,
                mOmniboxFocusStateSupplier,
                mLayoutStateProvider,
                mToolbarPositionSupplier,
                mBrowserControlsVisibilityManager,
                mScrimManager,
                new View(mActivity),
                new View(mActivity));
    }

    @After
    public void tearDown() {
        GlicKeyedServiceFactory.setForTesting(null);
    }

    @Test
    public void testInitialization_bindsAction() {
        assertNotNull(mCoordinator);
        verify(mActionRegistry, times(1)).get(ActionId.NEW_TAB);
        verify(mActionRegistry, times(1)).get(ActionId.TAB_SWITCHER);
    }

    @Test
    public void testActionBinding_setsClickListener() {
        AtomicBoolean clicked = new AtomicBoolean(false);
        Callback<View> onPressCallback = (v) -> clicked.set(true);
        PropertyModel actionModel = new PropertyModel.Builder(ActionProperties.BASE_KEYS).build();

        mActionSupplier.set(actionModel);

        // Verify the button is initialized.
        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);

        // No callback is assigned, so clicking does nothing.
        newTabButton.performClick();
        assertFalse(clicked.get());

        // Assign the callback and test again.
        actionModel.set(ActionProperties.ON_PRESS_CALLBACK, onPressCallback);
        newTabButton.performClick();
        assertTrue(clicked.get());
    }

    @Test
    public void testActionBinding_setsTooltipText() {
        PropertyModel actionModel = new PropertyModel.Builder(ActionProperties.BASE_KEYS).build();
        mActionSupplier.set(actionModel);

        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        assertNull(newTabButton.getTooltipText());

        actionModel.set(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "New Tab Tooltip");
        assertEquals("New Tab Tooltip", newTabButton.getTooltipText());
    }

    @Test
    public void testActionBinding_setsLongClickListener() {
        AtomicBoolean longClicked = new AtomicBoolean(false);
        Callback<View> onLongPressCallback = (v) -> longClicked.set(true);
        PropertyModel actionModel = new PropertyModel.Builder(ActionProperties.BASE_KEYS).build();

        mActionSupplier.set(actionModel);

        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);

        // No callback is assigned, long clicking does not trigger callback.
        newTabButton.performLongClick();
        assertFalse(longClicked.get());

        // Assign the callback and test again.
        actionModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, onLongPressCallback);
        newTabButton.performLongClick();
        assertTrue(longClicked.get());
    }

    @Test
    public void testExtraButton_bindsEligibleActionTooltipAndLongPress() {
        GlicEnabling.Natives glicEnablingMock = mock(GlicEnabling.Natives.class);
        GlicEnablingJni.setInstanceForTesting(glicEnablingMock);
        when(glicEnablingMock.isEnabledForProfile(any())).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        AtomicBoolean glicLongClicked = new AtomicBoolean(false);

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.TOOLTIP_TEXT_RESOLVER,
                                context -> "Ask Gemini Tooltip")
                        .with(
                                ActionProperties.ON_LONG_PRESS_CALLBACK,
                                (v) -> glicLongClicked.set(true))
                        .build();

        mGlicActionSupplier.set(glicModel);

        // Trigger profile update so mediator resolves GLIC as eligible candidate.
        mProfileSupplier.set(null);
        mProfileSupplier.set(mProfile);

        View extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        assertNotNull(extraButton);

        assertEquals("Ask Gemini Tooltip", extraButton.getTooltipText());

        extraButton.performLongClick();
        assertTrue(glicLongClicked.get());
    }

    @Test
    public void testDestroy() {
        PropertyModel actionModel = new PropertyModel.Builder(ActionProperties.BASE_KEYS).build();
        mActionSupplier.set(actionModel);
        clearInvocations(mVisibilityDelegate);

        assertTrue(mActionSupplier.hasObservers());
        mCoordinator.destroy();
        assertFalse(mActionSupplier.hasObservers());
        verify(mVisibilityDelegate, never()).onModelTokenChange();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testInitialization_withHomeButton_bindsHomeButton() {
        verify(mActionRegistry, times(1)).get(ActionId.HOME_BUTTON);

        View homeButton = mCoordinator.getView().findViewById(R.id.home_button);
        assertNotNull(homeButton);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false")
    public void testInitialization_withAppMenu_bindsAppMenu() {
        verify(mActionRegistry, times(1)).get(ActionId.APP_MENU);

        View menuButton = mCoordinator.getView().findViewById(R.id.menu_button);
        assertNotNull(menuButton);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_update_badge/true"
    })
    public void testInitialization_withAppMenuAndBadge_accessibilityClassName() {
        verify(mActionRegistry, times(1)).get(ActionId.APP_MENU);

        View menuButton = mCoordinator.getView().findViewById(R.id.menu_button);
        assertNotNull(menuButton);
        assertTrue(menuButton instanceof BottomBarAppMenu);
        assertEquals(Button.class.getName(), menuButton.getAccessibilityClassName().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true")
    public void testInitialization_withoutAppMenu_doesNotBindAppMenu() {
        verify(mActionRegistry, never()).get(ActionId.APP_MENU);

        View menuButton = mCoordinator.getView().findViewById(R.id.menu_button);
        assertNull(menuButton);
    }

    @Test
    public void testGetBackgroundColor() {
        int expectedColor =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedColor, mCoordinator.getBackgroundColor());
    }

    @Test
    public void testSetParent_UpdatesHost() {
        assertEquals(Host.TABBED, mCoordinator.getMediatorForTesting().getHostForTesting());

        mCoordinator.setParent(Host.HUB);
        assertEquals(Host.HUB, mCoordinator.getMediatorForTesting().getHostForTesting());

        mCoordinator.setParent(Host.TABBED);
        assertEquals(Host.TABBED, mCoordinator.getMediatorForTesting().getHostForTesting());
    }

    @Test
    public void testOmniboxFocusHidesBottomBar() {
        // Initially not focused, should be visible.
        verify(mVisibilityDelegate).onVisibilityChanged(true);

        // Focus omnibox, should hide.
        mOmniboxFocusStateSupplier.set(true);
        verify(mVisibilityDelegate).onVisibilityChanged(false);

        // Unfocus omnibox, should show again.
        mOmniboxFocusStateSupplier.set(false);
        verify(mVisibilityDelegate, times(2)).onVisibilityChanged(true);
    }

    @Test
    public void testExtraButton_WhenGlicEligible_ShowsGlicContentDescriptionAndTooltip() {
        GlicEnabling.setEnabledForTesting(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        AtomicBoolean glicClicked = new AtomicBoolean(false);

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .with(ActionProperties.ON_PRESS_CALLBACK, v -> glicClicked.set(true))
                        .build();

        mGlicActionSupplier.set(glicModel);

        mProfileSupplier.set(mProfile);

        View extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        assertNotNull(extraButton);
        assertEquals("Ask Gemini", extraButton.getContentDescription());
        assertEquals("Ask Gemini", extraButton.getTooltipText());

        extraButton.performClick();
        assertTrue("GLIC click callback should be triggered", glicClicked.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testExtraButton_WhenGlicPreferenceToggled_HidesAndShows() {
        GlicEnabling.setEnabledForTesting(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        AtomicBoolean glicClicked = new AtomicBoolean(false);

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .with(ActionProperties.ON_PRESS_CALLBACK, v -> glicClicked.set(true))
                        .build();

        mGlicActionSupplier.set(glicModel);

        mProfileSupplier.set(mProfile);

        View extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        View extraContainer = mCoordinator.getView().findViewById(R.id.extra_button_container);
        assertNotNull(extraButton);
        assertNotNull(extraContainer);
        assertEquals(View.VISIBLE, extraContainer.getVisibility());
        assertEquals("Ask Gemini", extraButton.getContentDescription());
        assertEquals("Ask Gemini", extraButton.getTooltipText());

        // Toggle GLIC button OFF via SharedPreferences.
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ false);

        // Extra button should be GONE.
        assertEquals(View.GONE, extraContainer.getVisibility());

        // Toggle GLIC button ON via SharedPreferences.
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ true);

        // Extra button should be VISIBLE again with GLIC.
        assertEquals(View.VISIBLE, extraContainer.getVisibility());
        assertEquals("Ask Gemini", extraButton.getContentDescription());
        assertEquals("Ask Gemini", extraButton.getTooltipText());

        extraButton.performClick();
        assertTrue("GLIC click callback should be triggered", glicClicked.get());
    }

    @Test
    public void testExtraButton_ColdStart_NullProfile_ExtraButtonHidden() {
        GlicEnabling.setEnabledForTesting(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        AtomicBoolean glicClicked = new AtomicBoolean(false);
        AtomicBoolean glicLongClicked = new AtomicBoolean(false);

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(
                                ActionProperties.TOOLTIP_TEXT_RESOLVER,
                                context -> "Ask Gemini Tooltip")
                        .with(ActionProperties.ON_PRESS_CALLBACK, (v) -> glicClicked.set(true))
                        .with(
                                ActionProperties.ON_LONG_PRESS_CALLBACK,
                                (v) -> glicLongClicked.set(true))
                        .build();
        mGlicActionSupplier.set(glicModel);

        // Recreate coordinator with null profile initially.
        mProfileSupplier.set(null);
        mCoordinator.destroy();
        mCoordinator = createCoordinator();
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        assertNull(extraButton);

        // Profile becomes available.
        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();
        extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        assertNotNull(extraButton);
        assertEquals(View.VISIBLE, extraButton.getVisibility());
        assertEquals("Ask Gemini", extraButton.getContentDescription());
        assertEquals("Ask Gemini Tooltip", extraButton.getTooltipText());

        extraButton.performClick();
        assertTrue("GLIC click callback should be triggered", glicClicked.get());

        extraButton.performLongClick();
        assertTrue("GLIC long-click callback should be triggered", glicLongClicked.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testExtraButton_GlicDisabled_ResolvesNone() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ false);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(
                                ActionProperties.TOOLTIP_TEXT_RESOLVER,
                                context -> "Ask Gemini Tooltip")
                        .build();

        mGlicActionSupplier.set(glicModel);

        mCoordinator.destroy();
        mCoordinator = createCoordinator();

        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getView().findViewById(R.id.extra_button);
        assertNull(extraButton);
        View extraContainer = mCoordinator.getView().findViewById(R.id.extra_button_container);
        assertNotNull(extraContainer);
        assertEquals(View.GONE, extraContainer.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/48")
    public void testBottomBarHeight() {
        assertEquals(48, mCoordinator.getView().getLayoutParams().height);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testBottomBarHeight_RemoveTopPaddingForBottomOmnibox() {
        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertEquals(56, mCoordinator.getView().getLayoutParams().height);
        assertEquals(8, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());

        mToolbarPositionSupplier.set(ControlsPosition.BOTTOM);
        assertEquals(48, mCoordinator.getView().getLayoutParams().height);
        assertEquals(0, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());

        // Switching to Hub host restores top padding and full height.
        mCoordinator.setParent(Host.HUB);
        assertEquals(56, mCoordinator.getView().getLayoutParams().height);
        assertEquals(8, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());

        // Returning to Tabbed host removes top padding again.
        mCoordinator.setParent(Host.TABBED);
        assertEquals(48, mCoordinator.getView().getLayoutParams().height);
        assertEquals(0, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true"
    })
    public void testMaybeShowBottomBarPromo_EducationEnabled_ClaimsSlot() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);

        assertTrue(mCoordinator.maybeShowBottomBarPromo(mProfile));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true"
    })
    public void testMaybeShowBottomBarPromo_EducationEnabled_DoesNotClaimSlot() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);

        assertFalse(mCoordinator.maybeShowBottomBarPromo(mProfile));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true"
    })
    public void testMaybeShowBottomBarPromo_EducationEnabled_IncognitoReturnsFalse() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(true);
        mTabSupplier.set(mTab);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);

        assertFalse(mCoordinator.maybeShowBottomBarPromo(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testMaybeShowBottomBarPromo_EducationDisabled_ReturnsFalse() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        assertFalse(mCoordinator.maybeShowBottomBarPromo(mProfile));
    }
}
