// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.function.Supplier;

/** Unit tests for {@link IncognitoIndicatorCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
public class IncognitoIndicatorCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Supplier<@Nullable Tracker> mTrackerSupplier;
    @Mock private Tracker mTracker;

    private Activity mActivity;
    private int mDefaultFallbackWidth;
    private IncognitoIndicatorCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        ToolbarLayout parentToolbar =
                (ToolbarLayout)
                        mActivity.getLayoutInflater().inflate(R.layout.toolbar_tablet, null);
        mActivity.setContentView(parentToolbar);
        when(mTrackerSupplier.get()).thenReturn(mTracker);
        mDefaultFallbackWidth =
                3 * mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);

        mCoordinator =
                new IncognitoIndicatorCoordinator(
                        parentToolbar,
                        mUserEducationHelper,
                        mTrackerSupplier,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        () ->
                                MultiWindowUtils.getInstanceCount(
                                        MultiInstanceManager.PersistedInstanceType.OFF_THE_RECORD),
                        /* visible= */ false);
        assertNull(
                "Indicator should not be inflated initially.",
                mCoordinator.getIncognitoIndicatorView());
    }

    @Test
    public void testOnIncognitoStateChanged_TogglesVisibility() {
        mCoordinator.setVisibility(/* visible= */ true);

        // Start not in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertNull(
                "Indicator should not be inflated when not in incognito.",
                mCoordinator.getIncognitoIndicatorView());

        // Transition to incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        View indicator = mCoordinator.getIncognitoIndicatorView();
        assertNotNull("Indicator should be inflated.", indicator);
        assertEquals(View.VISIBLE, indicator.getVisibility());

        // Transition back out of incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertEquals(View.GONE, indicator.getVisibility());
    }

    @Test
    public void testSetVisibility_TogglesVisibility() {
        // Start in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        View indicator = mCoordinator.getIncognitoIndicatorView();
        assertNotNull("Indicator should be inflated.", indicator);
        assertEquals(View.GONE, indicator.getVisibility());

        // Show toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ true);
        assertSame(indicator, mCoordinator.getIncognitoIndicatorView());
        assertEquals(View.VISIBLE, indicator.getVisibility());

        // Hide toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ false);
        assertEquals(View.GONE, indicator.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testUpdateVisibility_TabStripMigrationDisabled() {
        assertEquals(0, mCoordinator.updateVisibility(300));
        assertNull("Indicator should not be inflated.", mCoordinator.getIncognitoIndicatorView());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testUpdateVisibility_ToggleIncognito() {
        // Start not in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertEquals(0, mCoordinator.updateVisibility(500));
        assertNull("Indicator should not be inflated.", mCoordinator.getIncognitoIndicatorView());

        // Switch to incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        View indicator = mCoordinator.getIncognitoIndicatorView();
        assertNotNull("Indicator should be inflated.", indicator);
        // Force the indicator to measure to a width of 0 so that the fallback width is used.
        indicator.getLayoutParams().width = 0;
        assertEquals(
                "The coordinator should have consumed 3 times the button width by default.",
                mDefaultFallbackWidth,
                mCoordinator.updateVisibility(500));
        assertEquals(View.VISIBLE, indicator.getVisibility());

        assertEquals(
                "The coordinator should still consume 3 times the button width.",
                mDefaultFallbackWidth,
                mCoordinator.updateVisibility(500));
        assertEquals(View.VISIBLE, indicator.getVisibility());

        // Update the indicator's measured width.
        indicator.getLayoutParams().width = 100;

        assertEquals(
                "The coordinator should now consume the previously measured width of the"
                        + " indicator.",
                100,
                mCoordinator.updateVisibility(500));
        assertEquals(View.VISIBLE, indicator.getVisibility());

        // Hide the indicator when there isn't enough available width.
        assertEquals(
                "The coordinator should consume the remaining width, but not show.",
                50,
                mCoordinator.updateVisibility(50));
        assertEquals(View.GONE, indicator.getVisibility());
    }

    @Test
    public void testCreateAndShowMenu() {
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertNotNull("Indicator should be inflated.", mCoordinator.getIncognitoIndicatorView());

        MultiWindowUtils.setInstanceCountForTesting(1);

        ModelList modelList = mCoordinator.buildMenuItems(mActivity);
        mCoordinator.createAndShowMenu(mActivity, modelList);
        assertNotNull(mCoordinator.getMenuWindowForTesting());
        assertTrue(mCoordinator.getMenuWindowForTesting().isShowing());
    }

    @Test
    public void testBuildMenuItems() {
        MultiWindowUtils.setInstanceCountForTesting(5);

        ModelList items = mCoordinator.buildMenuItems(mActivity);
        assertEquals(1, items.size());
        assertEquals(
                R.id.close_all_incognito_windows_menu_id,
                items.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Menu item title is incorrect.",
                "Close 5 Incognito windows",
                items.get(0).model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    public void testSetVisibility_TriggersIPH() {
        // Start in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);

        // Show coordinator. This should trigger IPH.
        mCoordinator.setVisibility(/* visible= */ true);

        ArgumentCaptor<IphCommand> captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        IphCommand command = captor.getValue();
        assertEquals(
                FeatureConstants.IPH_INCOGNITO_INDICATOR_CLOSE_ALL_WINDOWS, command.featureName);
        assertEquals(mCoordinator.getIncognitoIndicatorView(), command.anchorView);

        // Hiding and showing again should trigger it again (though BE might block it,
        // coordinator should still request it).
        mCoordinator.setVisibility(/* visible= */ false);
        mCoordinator.setVisibility(/* visible= */ true);
        verify(mUserEducationHelper, times(2)).requestShowIph(any());
    }

    @Test
    public void testOnClick_NotifiesUsedEvent() {
        // Make it visible so onClick doesn't early return.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        mCoordinator.setVisibility(/* visible= */ true);

        // Trigger click.
        mCoordinator.getIncognitoIndicatorView().performClick();

        // Verify event notified.
        verify(mTracker).notifyEvent(EventConstants.INCOGNITO_INDICATOR_CLOSE_ALL_WINDOWS_USED);
    }
}
