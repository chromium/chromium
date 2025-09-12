// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for the Accessibility Settings menu's Seekbar. */
@RunWith(BaseJUnit4ClassRunner.class)
@Features.DisableFeatures({
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2,
    ContentFeatureList.SMART_ZOOM
})
public class AccessibilitySettingsSeekbarTest {
    private AccessibilitySettings mAccessibilitySettings;
    private PageZoomSeekbarPreference mPageZoomPref;

    @Rule
    public BlankUiTestActivitySettingsTestRule mSettingsActivityTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private BrowserContextHandle mContextHandleMock;

    @Mock private AccessibilitySettingsDelegate mDelegate;
    @Mock private AccessibilitySettingsDelegate.IntegerPreferenceDelegate mIntegerPrefMock;
    @Mock private AccessibilitySettingsDelegate.BooleanPreferenceDelegate mBoolPrefMock;
    @Mock private SettingsNavigation mSettingsNavigationMock;

    @Mock private HostZoomMapImpl.Natives mHostZoomMapBridgeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapBridgeMock);

        when(mDelegate.getBrowserContextHandle()).thenReturn(mContextHandleMock);
        when(mDelegate.getForceEnableZoomAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getReaderAccessibilityDelegate()).thenReturn(mBoolPrefMock);
        when(mDelegate.getTextSizeContrastAccessibilityDelegate()).thenReturn(mIntegerPrefMock);
        when(mDelegate.getSiteSettingsNavigation()).thenReturn(mSettingsNavigationMock);

        // Enable screen reader to display all settings options.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(true));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(true);

        mSettingsActivityTestRule.launchPreference(
                AccessibilitySettings.class,
                null,
                (fragment) -> {
                    ((AccessibilitySettings) fragment).setDelegate(mDelegate);
                });
        mAccessibilitySettings =
                (AccessibilitySettings) mSettingsActivityTestRule.getPreferenceFragment();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false));
        when(mDelegate.shouldShowImageDescriptionsSetting()).thenReturn(false);
    }

    // Tests related to Page Zoom feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_decrease_zoom_button)).perform(click());
        Assert.assertTrue(startingVal > mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setZoomValueForTesting(0);
                });
        onView(withId(R.id.page_zoom_decrease_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_increase_zoom_button)).perform(click());
        Assert.assertTrue(startingVal < mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setZoomValueForTesting(PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE);
                });
        onView(withId(R.id.page_zoom_increase_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_zoomSliderUpdatesValue() {
        getPageZoomPref();
        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_slider)).perform(ViewActions.swipeRight());
        Assert.assertNotEquals(startingVal, mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    // Helper methods.

    private static final BaseMatcher<View> sDisabled =
            new BaseMatcher<>() {
                @Override
                public boolean matches(Object o) {
                    return !((ChromeImageButton) o).isEnabled();
                }

                @Override
                public void describeTo(Description description) {
                    description.appendText("View was enabled, but should have been disabled.");
                }
            };

    private void getPageZoomPref() {
        mPageZoomPref =
                (PageZoomSeekbarPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        Assert.assertNotNull(mPageZoomPref);
        Assert.assertTrue("Page Zoom pref should be visible.", mPageZoomPref.isVisible());
    }
}
